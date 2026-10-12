#include "audio.h"

#include <math.h>

#include "alarm.h"
#include "bsp/esp32_p4_wifi6_touch_lcd_xc.h"
#include "esp_codec_dev.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

enum { SAMPLE_RATE = 22050, CHUNK_SAMPLES = 441 };
static const float PI = 3.14159265358979323846f;
typedef struct {
  bool start;
  device_config_alarm_sound_t sound;
  uint8_t volume;
} preview_command_t;
typedef enum { PLAY_IDLE, PLAY_PREVIEW, PLAY_RING } playback_t;

enum { BELL_MODES = 7, BELL_STRIKE_SAMPLES = SAMPLE_RATE / 21, BELL_ATTACK_SAMPLES = 128 };
static const float bell_frequencies[BELL_MODES] = {1610, 2440, 3370, 4690, 6220, 8010, 9560};
static const float bell_weights[BELL_MODES] = {.23f, .67f, 1, .49f, .30f, .16f, .05f};
static const float bell_decays[BELL_MODES] = {.08f, .12f, .105f, .075f, .05f, .029f, .016f};
typedef struct {
  float x, y, attack_x, attack_y;
  float step_x, step_y, attack_step_x, attack_step_y;
  float strike_x, strike_y;
} bell_mode_t;
typedef struct {
  bell_mode_t modes[2][BELL_MODES];
  float knock_x, knock_y, knock_step_x, knock_step_y;
  float noise_level, noise_decay;
  uint32_t noise_seed;
  uint64_t next_frame;
  uint16_t until_strike;
  uint8_t attack_remaining[2];
  uint8_t strike_index;
  bool active;
} bell_state_t;

static QueueHandle_t s_commands;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static wac_audio_snapshot_t s_status;
static bool s_initialized;

static void bell_reset(bell_state_t *bell)
{
  *bell = (bell_state_t){ .noise_seed = 1977, .active = true };
  const float attack_decay = expf(-1 / (.00023f * SAMPLE_RATE));
  for (unsigned b = 0; b < 2; ++b) {
    for (unsigned m = 0; m < BELL_MODES; ++m) {
      bell_mode_t *mode = &bell->modes[b][m];
      const float angle = 2 * PI * bell_frequencies[m] * (b ? 1.061f : 1) / SAMPLE_RATE;
      const float decay = expf(-1 / (bell_decays[m] * SAMPLE_RATE));
      mode->step_x = cosf(angle) * decay;
      mode->step_y = sinf(angle) * decay;
      mode->attack_step_x = mode->step_x * attack_decay;
      mode->attack_step_y = mode->step_y * attack_decay;
      const float phase = .07f * m + .13f * b;
      mode->strike_x = bell_weights[m] * cosf(phase);
      mode->strike_y = bell_weights[m] * sinf(phase);
    }
  }
  const float knock_angle = 2 * PI * 450 / SAMPLE_RATE;
  const float knock_decay = expf(-1 / (.005f * SAMPLE_RATE));
  bell->knock_step_x = cosf(knock_angle) * knock_decay;
  bell->knock_step_y = sinf(knock_angle) * knock_decay;
  bell->noise_decay = expf(-1 / (.0014f * SAMPLE_RATE));
}

/* Adding each impulse to its bell's resonators preserves every earlier tail
 * without retaining a list of strikes. Subtracting a quickly decaying copy
 * gives each new modal impulse its short attack, independently of old tails.
 * All coefficients are prepared above; one sample needs only fixed arithmetic. */
static float bell_sample(bell_state_t *bell)
{
  static const int8_t timing_offsets[8] = {31, -42, 19, -58, 48, -15, 63, -46};
  static const float strengths[8] = {.91f, 1.07f, 1.13f, .96f, 1.04f, .87f, 1.09f, .93f};
  if (!bell->until_strike) {
    const unsigned index = bell->strike_index;
    const float strength = strengths[index];
    for (unsigned m = 0; m < BELL_MODES; ++m) {
      bell_mode_t *mode = &bell->modes[index & 1][m];
      const float x = mode->strike_x * strength, y = mode->strike_y * strength;
      mode->x += x; mode->y += y;
      mode->attack_x = x; mode->attack_y = y;
    }
    bell->attack_remaining[index & 1] = BELL_ATTACK_SAMPLES;
    bell->knock_x += .06f * strength;
    bell->noise_level += .18f * strength;
    bell->until_strike = BELL_STRIKE_SAMPLES + timing_offsets[index];
    bell->strike_index = (index + 1) & 7;
  }
  float value = 0;
  for (unsigned b = 0; b < 2; ++b) {
    const bool attacking = bell->attack_remaining[b] != 0;
    for (unsigned m = 0; m < BELL_MODES; ++m) {
      bell_mode_t *mode = &bell->modes[b][m];
      value += mode->y - (attacking ? mode->attack_y : 0);
      const float x = mode->step_x * mode->x - mode->step_y * mode->y;
      mode->y = mode->step_y * mode->x + mode->step_x * mode->y;
      mode->x = x;
      if (attacking) {
        const float attack_x = mode->attack_step_x * mode->attack_x -
                               mode->attack_step_y * mode->attack_y;
        mode->attack_y = mode->attack_step_y * mode->attack_x +
                         mode->attack_step_x * mode->attack_y;
        mode->attack_x = attack_x;
      }
    }
    /* The subtracted attack is below 1e-11 by this point. Retiring it avoids
     * spending most samples on inaudible, eventually subnormal values. */
    if (attacking) --bell->attack_remaining[b];
  }
  value += bell->knock_y;
  const float knock_x = bell->knock_step_x * bell->knock_x -
                        bell->knock_step_y * bell->knock_y;
  bell->knock_y = bell->knock_step_y * bell->knock_x +
                  bell->knock_step_x * bell->knock_y;
  bell->knock_x = knock_x;
  bell->noise_seed = bell->noise_seed * 1664525u + 1013904223u;
  value += bell->noise_level * ((float)(bell->noise_seed >> 8) / 8388608 - 1);
  bell->noise_level *= bell->noise_decay;
  --bell->until_strike;
  ++bell->next_frame;
  /* The fixed modal/strike sequence settles to an 8400-sample cycle. Its
   * startup and settled modal peak, including any possible hammer-noise sign,
   * stay below 3.95. This scale gives about .805 before the common .82 gain,
   * leaving headroom after the existing codec's approximately 1.496 gain. */
  return value * .204f;
}

static void publish(bool preview, esp_err_t error, bool unavailable)
{
  portENTER_CRITICAL(&s_lock);
  s_status.preview_active = preview;
  s_status.error = error;
  if (unavailable) s_status.ready = false;
  portEXIT_CRITICAL(&s_lock);
}

/* These definitions are shared by preview and ringing. A finite limit clips
 * the preview's last tones exactly as the approved browser reference does. */
static float tone(uint32_t frame, uint32_t offset, float duration, float frequency,
                  float peak, unsigned shape, uint32_t limit)
{
  if (frame < offset || limit <= offset) return 0;
  duration = fminf(duration, (float)(limit - offset) / SAMPLE_RATE);
  const float age = (float)(frame - offset) / SAMPLE_RATE;
  if (age >= duration || duration <= 0) return 0;
  const float attack = fminf(.012f, duration / 3);
  float envelope;
  if (age < attack) envelope = peak * age / attack;
  else envelope = peak * fminf(1, (duration - age) / .02f);
  const float cycles = frequency * age;
  const float phase = cycles - floorf(cycles);
  /* Square and triangle shapes need only their position within one cycle. */
  const float wave = shape == 1 ? (phase <= .5f ? 1 : -1) :
                                (phase < .25f ? 4 * phase :
                                 phase < .75f ? 2 - 4 * phase : 4 * phase - 4);
  return wave * envelope;
}

static float sound_sample(device_config_alarm_sound_t sound, uint64_t frame, bool ringing)
{
  const uint32_t period = sound == DEVICE_CONFIG_ALARM_CLASSIC_BEEP ? SAMPLE_RATE * 11 / 10 :
                                                                    SAMPLE_RATE * 14 / 10;
  /* All pattern periods and note offsets are whole samples. Reduce the integer
   * frame before converting to float, so even a days-long ring retains each
   * sample and note boundary. Tone ages are likewise measured from their note. */
  const uint32_t local_frame = frame % period;
  if (!ringing && frame >= 5 * SAMPLE_RATE) return 0;
  const uint32_t limit = ringing ? UINT32_MAX : 5 * SAMPLE_RATE - (frame - local_frame);
  if (sound == DEVICE_CONFIG_ALARM_CLASSIC_BEEP)
    return tone(local_frame, 0, .18f, 880, .42f, 1, limit) +
           tone(local_frame, SAMPLE_RATE * 36 / 100, .18f, 880, .42f, 1, limit);
  float value = 0;
  const float frequencies[] = {440, 554.37f, 659.25f, 880};
  for (unsigned i = 0; i < 4; ++i)
    value += tone(local_frame, i * (SAMPLE_RATE * 24 / 100), .18f, frequencies[i], .65f, 2, limit);
  return value;
}

static void fill_chunk(int16_t *pcm, uint64_t frame, device_config_alarm_sound_t sound,
                       uint8_t volume, bool ringing, float ring_seconds, bell_state_t *bell)
{
  const bool classic_bell = sound == DEVICE_CONFIG_ALARM_CLASSIC_BELL;
  if (classic_bell && (!bell->active || bell->next_frame != frame)) {
    bell_reset(bell);
    bell->next_frame = frame;
  } else if (!classic_bell) bell->active = false;
  const float ramp = ringing ? .1f + .9f * fminf(1, ring_seconds / 30) : 1;
  const float level = (float)volume / 100;
  const float gain = level * level * .82f * ramp;
  for (unsigned i = 0; i < CHUNK_SAMPLES; ++i) {
    float sample = classic_bell ? (ringing || frame + i < 5 * SAMPLE_RATE ? bell_sample(bell) : 0) :
                                 sound_sample(sound, frame + i, ringing);
    sample *= gain;
    sample = fmaxf(-1, fminf(1, sample));
    pcm[i] = (int16_t)lrintf(sample * INT16_MAX);
  }
}

static bool same_occurrence(const wac_alarm_occurrence_t *a, const wac_alarm_occurrence_t *b)
{
  return a->valid == b->valid && a->utc == b->utc &&
         a->local_date == b->local_date && a->minute == b->minute;
}

static void audio_worker(void *argument)
{
  (void)argument;
  esp_codec_dev_handle_t speaker = NULL;
  bool creation_attempted = false, opened = false, was_ringing = false;
  wac_alarm_occurrence_t occurrence = {0};
  playback_t playback = PLAY_IDLE;
  preview_command_t preview = {0};
  uint64_t frame = 0;
  int64_t started = 0;
  int16_t pcm[CHUNK_SAMPLES];
  bell_state_t bell = {0};
  esp_err_t error = ESP_OK;
  for (;;) {
    wac_alarm_snapshot_t alarm;
    device_config_t config;
    wac_alarm_copy(&alarm);
    const esp_err_t config_error = device_config_copy(&config);
    const bool ringing = alarm.phase == WAC_ALARM_RINGING;
    const bool new_ring = ringing && (!was_ringing || !same_occurrence(&occurrence, &alarm.current));
    was_ringing = ringing;
    occurrence = alarm.current;

    preview_command_t command;
    portENTER_CRITICAL(&s_lock);
    const bool commanded = xQueueReceive(s_commands, &command, 0) == pdTRUE;
    if (commanded || ringing) s_status.preview_active = !ringing && commanded && command.start;
    portEXIT_CRITICAL(&s_lock);

    playback_t next = playback;
    bool restart = false;
    if (ringing) {
      if (new_ring) { next = PLAY_RING; restart = true; }
      else if (playback == PLAY_PREVIEW) next = PLAY_IDLE;
    } else if (commanded) {
      next = command.start ? PLAY_PREVIEW : PLAY_IDLE;
      if (command.start) { preview = command; restart = true; }
    } else if (playback == PLAY_RING) next = PLAY_IDLE;
    if (playback == PLAY_PREVIEW &&
        (frame >= 5 * SAMPLE_RATE || esp_timer_get_time() - started >= 5000000) && !restart)
      next = PLAY_IDLE;

    if (next != playback || restart) {
      if (opened) {
        const esp_err_t closed = esp_codec_dev_close(speaker);
        if (closed != ESP_OK) error = closed;
        opened = false;
      }
      playback = next;
      bell.active = false;
      frame = 0;
      started = esp_timer_get_time();
    }
    if (playback == PLAY_IDLE) {
      publish(false, error, false);
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    if (config_error != ESP_OK && playback == PLAY_RING) {
      error = config_error;
      if (opened) { esp_codec_dev_close(speaker); opened = false; }
      playback = PLAY_IDLE;
    } else if (!opened) {
      if (!creation_attempted) {
        creation_attempted = true;
        speaker = bsp_audio_codec_speaker_init();
      }
      if (!speaker) {
        error = ESP_ERR_INVALID_STATE;
        playback = PLAY_IDLE;
        publish(false, error, true);
        continue;
      }
      esp_codec_dev_sample_info_t format = {
        .bits_per_sample = 16, .channel = 1, .sample_rate = SAMPLE_RATE,
      };
      error = esp_codec_dev_open(speaker, &format);
      if (error == ESP_OK) error = esp_codec_dev_set_out_vol(speaker, 100);
      if (error == ESP_OK) {
        opened = true;
        started = esp_timer_get_time();
        /* Setup may have waited on the shared bus. Observe cancellation and
         * ring preemption again before queuing the first audible chunk. */
        wac_alarm_snapshot_t latest_alarm;
        wac_alarm_copy(&latest_alarm);
        portENTER_CRITICAL(&s_lock);
        preview_command_t pending;
        const bool pending_command = xQueuePeek(s_commands, &pending, 0) == pdTRUE;
        portEXIT_CRITICAL(&s_lock);
        if (pending_command || latest_alarm.phase != alarm.phase ||
            (ringing && !same_occurrence(&latest_alarm.current, &alarm.current))) continue;
      }
      else {
        /* Open sets internal flags before some failures; close that attempt. */
        esp_codec_dev_close(speaker);
        playback = PLAY_IDLE;
      }
    }
    if (playback == PLAY_IDLE) { publish(false, error, false); continue; }
    publish(playback == PLAY_PREVIEW, error, false);
    device_config_alarm_sound_t sound = playback == PLAY_RING ? config.alarm_sound : preview.sound;
    fill_chunk(pcm, frame, sound,
               playback == PLAY_RING ? config.alarm_volume : preview.volume,
               playback == PLAY_RING, (float)(esp_timer_get_time() - started) / 1000000, &bell);
    error = esp_codec_dev_write(speaker, pcm, sizeof(pcm));
    frame += CHUNK_SAMPLES;
    if (error != ESP_OK) {
      esp_codec_dev_close(speaker);
      opened = false;
      playback = PLAY_IDLE;
      publish(false, error, false);
    }
  }
}

esp_err_t wac_audio_init(void)
{
  if (s_initialized) return s_status.ready ? ESP_OK : s_status.error;
  s_initialized = true;
  s_commands = xQueueCreate(1, sizeof(preview_command_t));
  if (!s_commands) { s_status.error = ESP_ERR_NO_MEM; return s_status.error; }
  s_status.ready = true;
  if (xTaskCreate(audio_worker, "audio", 6144, NULL, 4, NULL) != pdPASS) {
    vQueueDelete(s_commands);
    s_commands = NULL;
    s_status.ready = false;
    s_status.error = ESP_ERR_NO_MEM;
    return s_status.error;
  }
  return ESP_OK;
}

static esp_err_t submit(preview_command_t command)
{
  portENTER_CRITICAL(&s_lock);
  const esp_err_t result = !s_status.ready ? ESP_ERR_INVALID_STATE :
                          xQueueOverwrite(s_commands, &command) == pdPASS ? ESP_OK : ESP_FAIL;
  portEXIT_CRITICAL(&s_lock);
  return result;
}

esp_err_t wac_audio_preview(device_config_alarm_sound_t sound, uint8_t volume)
{
  if (sound < DEVICE_CONFIG_ALARM_CLASSIC_BEEP || sound > DEVICE_CONFIG_ALARM_RISING_TONES || volume > 100)
    return ESP_ERR_INVALID_ARG;
  wac_alarm_snapshot_t alarm;
  wac_alarm_copy(&alarm);
  if (alarm.phase == WAC_ALARM_RINGING) return ESP_ERR_INVALID_STATE;
  return submit((preview_command_t){ .start = true, .sound = sound, .volume = volume });
}

esp_err_t wac_audio_stop_preview(void)
{
  return submit((preview_command_t){0});
}

void wac_audio_copy(wac_audio_snapshot_t *out)
{
  if (!out) return;
  wac_alarm_snapshot_t alarm;
  wac_alarm_copy(&alarm);
  portENTER_CRITICAL(&s_lock);
  *out = s_status;
  preview_command_t pending;
  if (s_commands && xQueuePeek(s_commands, &pending, 0) == pdTRUE)
    out->preview_active = pending.start;
  if (alarm.phase == WAC_ALARM_RINGING || !out->ready) out->preview_active = false;
  portEXIT_CRITICAL(&s_lock);
}
