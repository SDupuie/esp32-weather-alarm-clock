#include "audio.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_audio_simple_dec.h"
#include "esp_mp3_dec.h"

#include "alarm.h"
#include "bsp/esp32_p4_wifi6_touch_lcd_xc.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

enum { SAMPLE_RATE = 22050, CHUNK_SAMPLES = 441 };
static const char *TAG = "audio";
static const float PI = 3.14159265358979323846f;
typedef struct {
  bool start;
  device_config_alarm_sound_t sound;
  uint8_t volume;
} preview_command_t;
typedef enum { PLAY_IDLE, PLAY_PREVIEW, PLAY_RING, PLAY_MACHINE, PLAY_WEATHER } playback_t;

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
/* Independent from preview Stop, which ordinary editor navigation submits. */
static bool s_machine_command, s_machine_start;
static bool s_weather_requested;
static uint32_t s_weather_generation, s_weather_next_token;
static bool s_mp3_registered, s_spiffs_mounted;

enum { MP3_INPUT_BYTES = 2048, MP3_OUTPUT_BYTES = 4608, MACHINE_MAX_SAMPLES = 882 };
typedef struct {
  uint32_t seed;
  float pink[7], brown;
} noise_state_t;
typedef struct {
  FILE *file;
  const char *path;
  esp_audio_simple_dec_handle_t decoder;
  uint8_t *input, *decoded;
  int16_t *pcm;
  size_t input_size, output_size, output_offset;
  uint64_t samples, delivered;
  uint32_t rate;
  bool eof;
} recording_t;
typedef struct {
  recording_t recording;
  noise_state_t noise;
  device_config_sound_machine_sound_t sound;
  uint8_t timer_minutes;
  int64_t deadline;
  bool timer_started;
} machine_t;

/* soundmachine.app/playback.js noiseBuffer equations, using our uniform PRNG. */
static float noise_sample(noise_state_t *state, device_config_sound_machine_sound_t sound)
{
  uint32_t x = state->seed;
  x ^= x << 13; x ^= x >> 17; x ^= x << 5;
  state->seed = x;
  const float white = (float)(x >> 8) / 8388608 - 1;
  if (sound == DEVICE_CONFIG_SOUND_MACHINE_WHITE_NOISE) return white;
  if (sound == DEVICE_CONFIG_SOUND_MACHINE_BROWN_NOISE) {
    state->brown = (state->brown + .02f * white) / 1.02f;
    return state->brown * 3.5f;
  }
  /* Fixed pink-noise filter coefficients; no setup or shared work per sample. */
  float *b = state->pink;
  b[0] = .99886f * b[0] + white * .0555179f;
  b[1] = .99332f * b[1] + white * .0750759f;
  b[2] = .96900f * b[2] + white * .1538520f;
  b[3] = .86650f * b[3] + white * .3104856f;
  b[4] = .55000f * b[4] + white * .5329522f;
  b[5] = -.7616f * b[5] - white * .0168980f;
  const float pink = b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + white * .5362f;
  b[6] = white * .115926f;
  return pink * .11f;
}

static uint32_t be32(const uint8_t *bytes)
{
  return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
         ((uint32_t)bytes[2] << 8) | bytes[3];
}

/* These prepared files begin with a mono Layer III Xing/LAME metadata frame.
 * Tagged sample count N excludes encoder delay and end padding. The decoder
 * removes leading delay; cap its returned PCM at N to discard end padding. */
static bool recording_metadata(const uint8_t *data, size_t size, uint32_t *rate,
                               uint64_t *samples)
{
  if (size < 4) return false;
  const uint32_t header = be32(data);
  const unsigned version = (header >> 19) & 3, frequency = (header >> 10) & 3;
  const unsigned bitrate = (header >> 12) & 15;
  if ((header & 0xffe00000u) != 0xffe00000u || (version != 3 && version != 2) ||
      ((header >> 17) & 3) != 1 || ((header >> 6) & 3) != 3 || frequency == 3 ||
      bitrate == 0 || bitrate == 15) return false;
  static const uint16_t rates[] = {44100, 48000, 32000};
  *rate = rates[frequency] / (version == 3 ? 1 : 2);
  if (*rate != 44100 && *rate != 24000) return false;
  static const uint16_t bitrate1[] = {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320};
  static const uint16_t bitrate2[] = {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160};
  const size_t frame_bytes = (version == 3 ? 144000u * bitrate1[bitrate] :
                                            72000u * bitrate2[bitrate]) / *rate + ((header >> 9) & 1);
  size_t at = 4 + ((header & (1u << 16)) ? 0 : 2) + (version == 3 ? 17 : 9);
  if (frame_bytes > size || at + 8 > frame_bytes ||
      (memcmp(data + at, "Xing", 4) && memcmp(data + at, "Info", 4))) return false;
  const uint32_t flags = be32(data + at + 4);
  at += 8;
  if (!(flags & 1) || (flags & ~15u) || at + 4 > frame_bytes) return false;
  const uint32_t frames = be32(data + at);
  at += 4;
  if (flags & 2) at += 4;
  if (flags & 4) at += 100;
  if (flags & 8) at += 4;
  if (at + 24 > frame_bytes || memcmp(data + at, "LAME", 4)) return false;
  const unsigned delay = ((unsigned)data[at + 21] << 4) | (data[at + 22] >> 4);
  const unsigned padding = ((unsigned)(data[at + 22] & 15) << 8) | data[at + 23];
  const uint64_t count = (uint64_t)frames * (version == 3 ? 1152 : 576);
  if (!frames || count <= delay + padding) return false;
  *samples = count - delay - padding;
  return true;
}

static void recording_close(recording_t *recording)
{
  if (recording->decoder) esp_audio_simple_dec_close(recording->decoder);
  if (recording->file) fclose(recording->file);
  free(recording->input); free(recording->decoded); free(recording->pcm);
  *recording = (recording_t){0};
}

static bool machine_interrupted(void)
{
  wac_alarm_snapshot_t alarm;
  wac_alarm_copy(&alarm);
  portENTER_CRITICAL(&s_lock);
  preview_command_t preview;
  const bool interrupted = s_machine_command || s_weather_requested ||
    (xQueuePeek(s_commands, &preview, 0) == pdTRUE && preview.start);
  portEXIT_CRITICAL(&s_lock);
  return interrupted || alarm.phase == WAC_ALARM_RINGING;
}

static esp_err_t recording_open(recording_t *recording, device_config_sound_machine_sound_t sound)
{
  static const char *const paths[] = {"/spiffs/rain.mp3", "/spiffs/thunder.mp3", "/spiffs/ocean.mp3",
    "/spiffs/wind.mp3", "/spiffs/forest.mp3", "/spiffs/fire.mp3", "/spiffs/stream.mp3",
    "/spiffs/night.mp3", "/spiffs/bowl.mp3"};
  if (!s_spiffs_mounted) {
    const esp_err_t error = bsp_spiffs_mount();
    if (error != ESP_OK) return error;
    s_spiffs_mounted = true;
  }
  if (machine_interrupted()) return ESP_ERR_INVALID_STATE;
  if (!s_mp3_registered) {
    if (esp_mp3_dec_register() != ESP_AUDIO_ERR_OK) return ESP_FAIL;
    s_mp3_registered = true;
  }
  if (machine_interrupted()) return ESP_ERR_INVALID_STATE;
  recording->input = malloc(MP3_INPUT_BYTES);
  recording->decoded = malloc(MP3_OUTPUT_BYTES);
  recording->pcm = malloc(MACHINE_MAX_SAMPLES * sizeof(int16_t));
  if (!recording->input || !recording->decoded || !recording->pcm) return ESP_ERR_NO_MEM;
  recording->path = paths[sound];
  recording->file = fopen(recording->path, "rb");
  if (!recording->file) return ESP_FAIL;
  if (machine_interrupted()) return ESP_ERR_INVALID_STATE;
  const size_t size = fread(recording->input, 1, MP3_INPUT_BYTES, recording->file);
  if (ferror(recording->file) || !recording_metadata(recording->input, size,
                                                  &recording->rate, &recording->samples)) return ESP_FAIL;
  if (fseek(recording->file, 0, SEEK_SET)) return ESP_FAIL;
  if (machine_interrupted()) return ESP_ERR_INVALID_STATE;
  esp_audio_simple_dec_cfg_t config = { .dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_MP3,
    .dec_cfg = NULL, .cfg_size = 0, .use_frame_dec = false };
  return esp_audio_simple_dec_open(&config, &recording->decoder) ==
         ESP_AUDIO_ERR_OK ? ESP_OK : ESP_FAIL;
}

static esp_err_t recording_decode(recording_t *recording)
{
  /* Preserve unconsumed encoded bytes across reads and decoder calls. */
  unsigned stalled = 0;
  for (;;) {
    if (machine_interrupted()) return ESP_ERR_INVALID_STATE;
    if (!recording->eof && recording->input_size < MP3_INPUT_BYTES) {
      const size_t wanted = MP3_INPUT_BYTES - recording->input_size;
      const size_t count = fread(recording->input + recording->input_size, 1, wanted, recording->file);
      if (ferror(recording->file)) return ESP_FAIL;
      recording->input_size += count;
      if (count < wanted) recording->eof = true;
      else {
        /* Detect exact-sized final reads too, without sacrificing an input byte. */
        const int next = fgetc(recording->file);
        if (next == EOF) { if (ferror(recording->file)) return ESP_FAIL; recording->eof = true; }
        else if (ungetc(next, recording->file) == EOF) return ESP_FAIL;
      }
      if (machine_interrupted()) return ESP_ERR_INVALID_STATE;
    }
    esp_audio_simple_dec_raw_t raw = { .buffer = recording->input, .len = recording->input_size,
                                      .eos = recording->eof };
    esp_audio_simple_dec_out_t output = { .buffer = recording->decoded, .len = MP3_OUTPUT_BYTES };
    const esp_audio_err_t result = esp_audio_simple_dec_process(recording->decoder, &raw, &output);
    if (machine_interrupted()) return ESP_ERR_INVALID_STATE;
    if (result == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
      if (!output.needed_size || output.needed_size > MP3_OUTPUT_BYTES || ++stalled >= 2)
        return ESP_FAIL;
      continue; /* Do not advance input on an output-capacity retry. */
    }
    if (result != ESP_AUDIO_ERR_OK || raw.consumed > recording->input_size ||
        output.decoded_size > MP3_OUTPUT_BYTES || output.needed_size > MP3_OUTPUT_BYTES ||
        raw.frame_recover != ESP_AUDIO_SIMPLE_DEC_RECOVERY_NONE ||
        (output.decoded_size & 1)) return ESP_FAIL;
    if (raw.consumed) {
      recording->input_size -= raw.consumed;
      memmove(recording->input, recording->input + raw.consumed, recording->input_size);
    }
    if (output.decoded_size) {
      esp_audio_simple_dec_info_t info;
      if (esp_audio_simple_dec_get_info(recording->decoder, &info) != ESP_AUDIO_ERR_OK ||
          info.sample_rate != recording->rate || info.bits_per_sample != 16 || info.channel != 1)
        return ESP_FAIL;
      recording->output_size = output.decoded_size / sizeof(int16_t);
      const uint64_t remaining = recording->samples - recording->delivered;
      if (recording->output_size > remaining) recording->output_size = remaining;
      recording->output_offset = 0;
      return ESP_OK;
    }
    if (!raw.consumed && ++stalled >= 2) return ESP_FAIL;
    if (raw.consumed) stalled = 0;
    if (recording->eof && !recording->input_size) return ESP_FAIL; /* Shorter than tagged N. */
  }
}

static esp_err_t recording_fill(recording_t *recording)
{
  /* Fill a 20 ms write even across a repeat, joining tail and head without
   * inserting silence when the file ends partway through the chunk. */
  const unsigned count = recording->rate / 50;
  unsigned filled = 0;
  while (filled < count) {
    if (machine_interrupted()) return ESP_ERR_INVALID_STATE;
    if (recording->delivered == recording->samples) {
      /* Reopening the same name uses SPIFFS's retained header-page lookup.
       * Seeking backward can scan the partition before finding that page. */
      FILE *consumed = recording->file;
      recording->file = NULL; /* fclose consumes the stream even on failure. */
      if (fclose(consumed)) return ESP_FAIL;
      recording->file = fopen(recording->path, "rb");
      if (!recording->file) return ESP_FAIL;
      if (machine_interrupted()) return ESP_ERR_INVALID_STATE;
      if (esp_audio_simple_dec_reset(recording->decoder) != ESP_AUDIO_ERR_OK) return ESP_FAIL;
      recording->input_size = recording->output_size = recording->output_offset = 0;
      recording->delivered = 0; recording->eof = false;
      if (machine_interrupted()) return ESP_ERR_INVALID_STATE;
    }
    if (recording->output_offset == recording->output_size) {
      const esp_err_t error = recording_decode(recording);
      if (error != ESP_OK) return error;
    }
    size_t take = recording->output_size - recording->output_offset;
    if (take > count - filled) take = count - filled;
    memcpy(recording->pcm + filled, (int16_t *)recording->decoded + recording->output_offset,
           take * sizeof(int16_t));
    recording->output_offset += take; recording->delivered += take; filled += take;
  }
  return ESP_OK;
}

static void machine_publish(const machine_t *machine, bool active, bool pending)
{
  /* Playback duration uses elapsed time, independent of RTC/network corrections. */
  const int64_t remaining = machine->deadline - esp_timer_get_time();
  portENTER_CRITICAL(&s_lock);
  s_status.machine_active = active;
  s_status.machine_pending = pending;
  s_status.machine_continuous = (active || pending) && !machine->timer_minutes;
  s_status.machine_remaining_seconds = (active || pending) && machine->timer_minutes ?
    (machine->timer_started ? (remaining > 0 ? (uint32_t)((remaining + 999999) / 1000000) : 0) :
                             machine->timer_minutes * 60u) : 0;
  portEXIT_CRITICAL(&s_lock);
}

static float machine_gain(uint8_t volume, const machine_t *machine)
{
  const float level = volume / 100.0f;
  const float fade = machine->timer_minutes && machine->timer_started ?
    fmaxf(0, fminf(1, (float)(machine->deadline - esp_timer_get_time()) / 30000000)) : 1;
  /* Retain the base headroom for the codec's approximately 1.496 gain.
   * Fixed playback adjustments are combined once per chunk, outside the sample loop. */
  const float gain = level * level * .66f * fade;
  if (machine->sound == DEVICE_CONFIG_SOUND_MACHINE_WHITE_NOISE) return gain * .25118864f;
  if (machine->sound >= DEVICE_CONFIG_SOUND_MACHINE_PINK_NOISE) return gain * .50118723f;
  if (machine->sound == DEVICE_CONFIG_SOUND_MACHINE_NIGHT ||
      machine->sound == DEVICE_CONFIG_SOUND_MACHINE_CAMPFIRE) return gain * 5.9497676f;
  if (machine->sound == DEVICE_CONFIG_SOUND_MACHINE_THUNDER) return gain * 1.1376273f;
  if (machine->sound == DEVICE_CONFIG_SOUND_MACHINE_SINGING_BOWL) return gain * 1.3803843f;
  return gain * 1.8814817f;
}

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

/* Integer half-second positions make the approved gaps exact. */
static float weather_sample(wac_audio_weather_kind_t kind, uint32_t frame)
{
  const uint32_t half = SAMPLE_RATE / 2;
  static const uint8_t warning_start[] = {0, 5, 8, 11, 16, 19};
  static const uint8_t warning_length[] = {4, 2, 2, 4, 2, 2};
  const unsigned bursts = kind == WAC_AUDIO_WEATHER_WATCH ? 3 : 6;
  for (unsigned burst = 0; burst < bursts; ++burst) {
    const uint32_t start = kind == WAC_AUDIO_WEATHER_WATCH ? burst * SAMPLE_RATE :
      warning_start[burst] * half;
    const uint32_t count = kind == WAC_AUDIO_WEATHER_WATCH ? half :
      warning_length[burst] * half;
    if (frame < start || frame >= start + count) continue;
    const uint32_t age = frame - start;
    const float seconds = (float)age / SAMPLE_RATE;
    const float envelope = fminf(1, fminf((float)age / (SAMPLE_RATE * .012f),
                                       (float)(count - age) / (SAMPLE_RATE * .02f)));
    /* Bell's bounded pre-gain peak is approximately .805. Dual tones share
     * that magnitude range; the ordinary .82 alarm gain remains downstream. */
    if (kind == WAC_AUDIO_WEATHER_WATCH)
      return .805f * envelope * sinf(2 * PI * 1050 * seconds);
    return .4025f * envelope * (sinf(2 * PI * 853 * seconds) + sinf(2 * PI * 960 * seconds));
  }
  return 0;
}

static uint32_t weather_frames(wac_audio_weather_kind_t kind)
{
  return kind == WAC_AUDIO_WEATHER_WATCH ? SAMPLE_RATE * 5 / 2 : SAMPLE_RATE * 21 / 2;
}

static bool weather_valid(uint32_t generation)
{
  wac_alarm_snapshot_t alarm;
  wac_alarm_copy(&alarm);
  portENTER_CRITICAL(&s_lock);
  const bool valid = s_weather_requested && generation == s_weather_generation;
  portEXIT_CRITICAL(&s_lock);
  return valid && alarm.phase != WAC_ALARM_RINGING;
}

static void weather_finish(uint32_t token, wac_audio_weather_state_t state, esp_err_t error)
{
  portENTER_CRITICAL(&s_lock);
  if (s_status.weather_token == token &&
      (s_status.weather_state == WAC_AUDIO_WEATHER_PENDING ||
       s_status.weather_state == WAC_AUDIO_WEATHER_PLAYING ||
       (state == WAC_AUDIO_WEATHER_FAILED &&
        (s_status.weather_state == WAC_AUDIO_WEATHER_FINISHED ||
         s_status.weather_state == WAC_AUDIO_WEATHER_CANCELED)))) {
    s_status.weather_state = state;
    s_status.weather_error = error;
    s_weather_requested = false;
    ++s_weather_generation;
  }
  portEXIT_CRITICAL(&s_lock);
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
  machine_t machine = {0};
  bool machine_paused = false;
  uint32_t weather_generation = 0, weather_token = 0;
  wac_audio_weather_kind_t weather_kind = WAC_AUDIO_WEATHER_WATCH;
  esp_err_t weather_saved_error = ESP_OK;
  esp_err_t error = ESP_OK;
  for (;;) {
    const char *operation = "playback";
    wac_alarm_snapshot_t alarm;
    device_config_t config;
    wac_alarm_copy(&alarm);
    const esp_err_t config_error = device_config_copy(&config);
    const bool ringing = alarm.phase == WAC_ALARM_RINGING;
    /* Start once per ringing occurrence, rather than retrying failed playback
     * on every worker iteration while the same alarm remains ringing. */
    const bool new_ring = ringing && (!was_ringing || !same_occurrence(&occurrence, &alarm.current));
    was_ringing = ringing;
    occurrence = alarm.current;

    preview_command_t command;
    bool machine_start;
    portENTER_CRITICAL(&s_lock);
    const bool commanded = xQueueReceive(s_commands, &command, 0) == pdTRUE;
    const bool machine_commanded = s_machine_command;
    machine_start = s_machine_start;
    s_machine_command = false;
    const bool weather_requested = s_weather_requested;
    const uint32_t requested_generation = s_weather_generation;
    const uint32_t requested_token = s_status.weather_token;
    const wac_audio_weather_state_t requested_state = s_status.weather_state;
    const wac_audio_weather_kind_t requested_kind = s_status.weather_kind;
    if (commanded || ringing) s_status.preview_active = !ringing && commanded && command.start;
    portEXIT_CRITICAL(&s_lock);

    if (!weather_requested && requested_state == WAC_AUDIO_WEATHER_PENDING && playback != PLAY_WEATHER)
      weather_finish(requested_token, WAC_AUDIO_WEATHER_CANCELED, ESP_OK);
    playback_t next = playback;
    bool restart = false, fresh_machine = false, resuming_machine = false;
    if (machine_commanded && !machine_start) machine_paused = false;
    if (machine_paused && config_error == ESP_OK) {
      if (machine.timer_minutes != config.sound_machine_timer_minutes) {
        machine.timer_minutes = config.sound_machine_timer_minutes;
        machine.deadline = esp_timer_get_time() + (int64_t)machine.timer_minutes * 60000000;
      }
      if (machine.timer_minutes && machine.timer_started && esp_timer_get_time() >= machine.deadline)
        machine_paused = false;
      if (machine.sound != config.sound_machine_sound) {
        recording_close(&machine.recording);
        machine.sound = config.sound_machine_sound;
        machine.noise = (noise_state_t){ .seed = 0x9e3779b9u };
      }
    }
    if (ringing) {
      /* A wake-up alarm cancels Sound Machine resumption. Weather audio below
       * instead retains its session and running sleep-timer deadline. */
      machine_paused = false;
      if (weather_requested || playback == PLAY_WEATHER)
        weather_finish(requested_token, WAC_AUDIO_WEATHER_SKIPPED_RING, ESP_OK);
      if (new_ring) { next = PLAY_RING; restart = true; }
      else if (playback == PLAY_PREVIEW || playback == PLAY_MACHINE || playback == PLAY_WEATHER)
        next = PLAY_IDLE;
    } else if (weather_requested && !(playback == PLAY_WEATHER &&
               frame >= weather_frames(weather_kind))) {
      if (playback != PLAY_WEATHER) {
        machine_paused = (playback == PLAY_MACHINE && !(machine_commanded && !machine_start)) ||
                         (machine_commanded && machine_start);
        if (playback != PLAY_MACHINE && machine_paused) {
          machine.sound = config.sound_machine_sound;
          machine.timer_minutes = config.sound_machine_timer_minutes;
          machine.timer_started = false;
          machine.noise = (noise_state_t){ .seed = 0x9e3779b9u };
        }
        weather_saved_error = error;
        weather_generation = requested_generation;
        weather_token = requested_token;
        weather_kind = requested_kind;
        next = PLAY_WEATHER;
      }
    } else if (playback == PLAY_WEATHER) {
      weather_finish(weather_token, weather_requested ? WAC_AUDIO_WEATHER_FINISHED :
                     WAC_AUDIO_WEATHER_CANCELED, ESP_OK);
      fresh_machine = machine_commanded && machine_start;
      resuming_machine = machine_paused && !fresh_machine;
      next = machine_paused || fresh_machine ? PLAY_MACHINE : PLAY_IDLE;
      machine_paused = false;
      if (commanded && command.start) {
        next = PLAY_PREVIEW; preview = command; restart = true;
        resuming_machine = fresh_machine = false;
      }
    } else if (commanded && command.start) {
      next = PLAY_PREVIEW; preview = command; restart = true;
    } else {
      /* Preview navigation cannot stop the machine or weather playback. */
      if (commanded && playback == PLAY_PREVIEW) next = PLAY_IDLE;
      if (playback == PLAY_RING) next = PLAY_IDLE;
      if (machine_commanded) {
        if (!machine_start && playback == PLAY_MACHINE) next = PLAY_IDLE;
        else if (machine_start && next != PLAY_PREVIEW && next != PLAY_RING) {
          next = PLAY_MACHINE; restart = fresh_machine = true;
        }
      }
    }
    if (playback == PLAY_WEATHER && !machine_paused && !resuming_machine)
      recording_close(&machine.recording);
    if (machine_paused) machine_publish(&machine, true, false);
    if (next == PLAY_PREVIEW && playback == PLAY_PREVIEW &&
        (frame >= 5 * SAMPLE_RATE || esp_timer_get_time() - started >= 5000000) && !restart)
      next = PLAY_IDLE;
    if (next == PLAY_MACHINE && config_error != ESP_OK) {
      ESP_LOGE(TAG, "Sound Machine settings unavailable: %s", esp_err_to_name(config_error));
      next = PLAY_IDLE; error = config_error;
    }
    if (next == PLAY_MACHINE && !fresh_machine) {
      if (machine.timer_minutes != config.sound_machine_timer_minutes) {
        machine.timer_minutes = config.sound_machine_timer_minutes;
        machine.deadline = esp_timer_get_time() + (int64_t)machine.timer_minutes * 60000000;
      }
      if (machine.timer_minutes && machine.timer_started && esp_timer_get_time() >= machine.deadline)
        next = PLAY_IDLE;
      else if (machine.sound != config.sound_machine_sound) restart = true;
    }

    if (next != playback || restart) {
      const bool weather_transition = playback == PLAY_WEATHER || next == PLAY_WEATHER;
      if (next != PLAY_MACHINE && !machine_paused) machine_publish(&machine, false, false);
      if (opened) {
        const esp_err_t closed = esp_codec_dev_close(speaker);
        if (closed != ESP_OK) {
          ESP_LOGE(TAG, "Speaker close failed: %s", esp_err_to_name(closed));
          error = closed;
          if (playback == PLAY_WEATHER || next == PLAY_WEATHER) {
            weather_finish(weather_token, WAC_AUDIO_WEATHER_FAILED, closed);
            machine_paused = resuming_machine = false;
            if (next != PLAY_RING) next = PLAY_IDLE;
          }
        }
        opened = false;
      }
      if ((!machine_paused && !resuming_machine) ||
          (next == PLAY_WEATHER && machine.recording.input && !machine.recording.decoder))
        recording_close(&machine.recording);
      if (fresh_machine) {
        machine.timer_minutes = config.sound_machine_timer_minutes;
        machine.timer_started = false;
        machine.deadline = 0;
      }
      if (next == PLAY_MACHINE && !resuming_machine) {
        machine.sound = config.sound_machine_sound;
        machine.noise = (noise_state_t){ .seed = 0x9e3779b9u };
      }
      if (weather_transition && next != PLAY_WEATHER) error = weather_saved_error;
      playback = next;
      bell.active = false;
      frame = 0;
      started = esp_timer_get_time();
    }
    if (playback == PLAY_IDLE) {
      machine_publish(&machine, false, false);
      publish(false, error, false);
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    if (config_error != ESP_OK && playback == PLAY_WEATHER) {
      error = config_error;
      goto playback_failed;
    }
    if (config_error != ESP_OK && playback == PLAY_RING) {
      ESP_LOGE(TAG, "Alarm audio settings unavailable: %s", esp_err_to_name(config_error));
      error = config_error;
      if (opened) {
        const esp_err_t closed = esp_codec_dev_close(speaker);
        if (closed != ESP_OK) ESP_LOGE(TAG, "Speaker close failed: %s", esp_err_to_name(closed));
        opened = false;
      }
      playback = PLAY_IDLE;
    } else if (!opened) {
      if (playback == PLAY_MACHINE) {
        machine_publish(&machine, false, true);
        if (machine.sound < DEVICE_CONFIG_SOUND_MACHINE_WHITE_NOISE && !machine.recording.file) {
          operation = "recording setup";
          error = recording_open(&machine.recording, machine.sound);
          if (error != ESP_OK) {
            if (machine_interrupted()) { error = ESP_OK; continue; }
            goto playback_failed;
          }
        }
        if (machine_interrupted()) continue;
      }
      if (!creation_attempted) {
        creation_attempted = true;
        speaker = bsp_audio_codec_speaker_init();
        if (!speaker) ESP_LOGE(TAG, "Speaker creation failed; audio unavailable until restart");
      }
      if (playback == PLAY_WEATHER && !weather_valid(weather_generation)) continue;
      if (!speaker) {
        error = ESP_ERR_INVALID_STATE;
        if (playback == PLAY_WEATHER) {
          weather_finish(weather_token, WAC_AUDIO_WEATHER_FAILED, error);
          machine_paused = false;
          error = weather_saved_error;
        }
        recording_close(&machine.recording);
        playback = PLAY_IDLE;
        machine_publish(&machine, false, false);
        publish(false, error, true);
        continue;
      }
      esp_codec_dev_sample_info_t format = {
        .bits_per_sample = 16, .channel = 1,
        .sample_rate = playback == PLAY_MACHINE && machine.recording.file ? machine.recording.rate : SAMPLE_RATE,
      };
      if (playback == PLAY_MACHINE && machine_interrupted()) continue;
      if (playback == PLAY_WEATHER && !weather_valid(weather_generation)) continue;
      operation = "speaker open";
      error = esp_codec_dev_open(speaker, &format);
      if (error == ESP_OK && playback == PLAY_MACHINE && machine_interrupted()) {
        opened = true;
        continue;
      }
      if (error == ESP_OK && playback == PLAY_WEATHER && !weather_valid(weather_generation)) {
        opened = true;
        continue;
      }
      if (error == ESP_OK) {
        operation = "speaker volume setup";
        error = esp_codec_dev_set_out_vol(speaker, 100);
      }
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
        const bool pending_weather = s_weather_requested;
        portEXIT_CRITICAL(&s_lock);
        if (playback == PLAY_MACHINE) {
          /* Observe applied sound/timer changes after slow setup, too. */
          continue;
        }
        if (playback == PLAY_WEATHER) {
          if (!weather_valid(weather_generation)) continue;
        } else if (pending_command || pending_weather || latest_alarm.phase != alarm.phase ||
            (ringing && !same_occurrence(&latest_alarm.current, &alarm.current))) continue;
      }
      else {
        /* Open sets internal flags before some failures; close that attempt. */
        const esp_err_t closed = esp_codec_dev_close(speaker);
        if (closed != ESP_OK) ESP_LOGE(TAG, "Speaker close failed: %s", esp_err_to_name(closed));
        goto playback_failed;
      }
    }
    if (playback == PLAY_IDLE) { publish(false, error, false); continue; }
    if (playback == PLAY_MACHINE) {
      if (machine_interrupted()) continue;
      int16_t *output = pcm;
      unsigned count = CHUNK_SAMPLES;
      if (machine.recording.file) {
        operation = "recording read/decode";
        error = recording_fill(&machine.recording);
        if (error != ESP_OK) {
          if (machine_interrupted()) { error = ESP_OK; continue; }
          goto playback_failed;
        }
        output = machine.recording.pcm;
        count = machine.recording.rate / 50;
      }
      if (!machine.timer_started) {
        machine.timer_started = true;
        machine.deadline = esp_timer_get_time() + (int64_t)machine.timer_minutes * 60000000;
      }
      const float gain = machine_gain(config.sound_machine_volume, &machine);
      /* Apply all output gains before the final 16-bit range guard. Generator
       * filter state is untouched, and lowering volume can avoid saturation. */
      for (unsigned i = 0; i < count; ++i) {
        const float sample = machine.recording.file ? output[i] * gain :
          noise_sample(&machine.noise, machine.sound) * gain * INT16_MAX;
        output[i] = (int16_t)lrintf(fmaxf(INT16_MIN, fminf(INT16_MAX, sample)));
      }
      if (machine_interrupted()) continue;
      if (machine.timer_minutes && esp_timer_get_time() >= machine.deadline) {
        error = ESP_OK;
        goto playback_failed;
      }
      operation = "speaker write";
      error = esp_codec_dev_write(speaker, output, count * sizeof(int16_t));
      if (error != ESP_OK) goto playback_failed;
      machine_publish(&machine, true, false);
      publish(false, ESP_OK, false);
      continue;
    }
    if (playback == PLAY_WEATHER) {
      if (!weather_valid(weather_generation)) continue;
      const float level = config.alarm_volume / 100.0f;
      const float gain = level * level * .82f;
      for (unsigned i = 0; i < CHUNK_SAMPLES; ++i)
        pcm[i] = (int16_t)lrintf(fmaxf(-1, fminf(1,
          weather_sample(weather_kind, (uint32_t)frame + i) * gain)) * INT16_MAX);
      if (!weather_valid(weather_generation)) continue;
      portENTER_CRITICAL(&s_lock);
      const bool valid = s_weather_requested && s_weather_generation == weather_generation;
      if (valid) {
        if (s_status.weather_state == WAC_AUDIO_WEATHER_PENDING)
          s_status.weather_started_us = esp_timer_get_time();
        s_status.weather_state = WAC_AUDIO_WEATHER_PLAYING;
      }
      portEXIT_CRITICAL(&s_lock);
      if (!valid) continue;
      operation = "speaker write";
      error = esp_codec_dev_write(speaker, pcm, sizeof(pcm));
      frame += CHUNK_SAMPLES;
      if (error == ESP_OK) continue;
      goto playback_failed;
    }
    if (playback == PLAY_PREVIEW) {
      portENTER_CRITICAL(&s_lock);
      const bool weather_pending = s_weather_requested;
      portEXIT_CRITICAL(&s_lock);
      if (weather_pending) continue;
    }
    publish(playback == PLAY_PREVIEW, error, false);
    device_config_alarm_sound_t sound = playback == PLAY_RING ? config.alarm_sound : preview.sound;
    fill_chunk(pcm, frame, sound,
               playback == PLAY_RING ? config.alarm_volume : preview.volume,
               playback == PLAY_RING, (float)(esp_timer_get_time() - started) / 1000000, &bell);
    operation = "speaker write";
    error = esp_codec_dev_write(speaker, pcm, sizeof(pcm));
    frame += CHUNK_SAMPLES;
    if (error == ESP_OK) continue;
playback_failed:
    if (error != ESP_OK) {
      ESP_LOGE(TAG, "%s playback failed during %s: %s",
               playback == PLAY_RING ? "Alarm" : playback == PLAY_PREVIEW ? "Preview" :
               playback == PLAY_WEATHER ? "Weather" : "Sound Machine",
               operation, esp_err_to_name(error));
    }
    if (opened) {
      const esp_err_t closed = esp_codec_dev_close(speaker);
      if (closed != ESP_OK) ESP_LOGE(TAG, "Speaker close failed: %s", esp_err_to_name(closed));
      opened = false;
    }
    if (playback == PLAY_WEATHER) {
      weather_finish(weather_token, WAC_AUDIO_WEATHER_FAILED, error);
      error = weather_saved_error;
    }
    machine_paused = false;
    recording_close(&machine.recording);
    playback = PLAY_IDLE;
    machine_publish(&machine, false, false);
    publish(false, error, false);
  }
}

esp_err_t wac_audio_init(void)
{
  if (s_initialized) return s_status.ready ? ESP_OK :
    s_status.error != ESP_OK ? s_status.error : ESP_ERR_INVALID_STATE;
  s_initialized = true;
  s_commands = xQueueCreate(1, sizeof(preview_command_t));
  if (!s_commands) { s_status.error = ESP_ERR_NO_MEM; return s_status.error; }
  s_status.ready = true;
  if (xTaskCreatePinnedToCore(audio_worker, "audio", 6144, NULL, 4, NULL, 1) != pdPASS) {
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
  const esp_err_t result = !s_status.ready || (command.start && s_weather_requested) ? ESP_ERR_INVALID_STATE :
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

esp_err_t wac_audio_sound_machine_play(void)
{
  wac_alarm_snapshot_t alarm;
  wac_alarm_copy(&alarm);
  if (alarm.phase == WAC_ALARM_RINGING) return ESP_ERR_INVALID_STATE;
  portENTER_CRITICAL(&s_lock);
  preview_command_t pending;
  const bool preview = s_status.preview_active ||
    (s_commands && xQueuePeek(s_commands, &pending, 0) == pdTRUE && pending.start);
  const esp_err_t error = !s_status.ready || preview || s_weather_requested ? ESP_ERR_INVALID_STATE : ESP_OK;
  if (error == ESP_OK) {
    s_machine_command = true; s_machine_start = true;
    s_status.machine_pending = true;
    s_status.error = ESP_OK;
  }
  portEXIT_CRITICAL(&s_lock);
  return error;
}

esp_err_t wac_audio_sound_machine_stop(void)
{
  portENTER_CRITICAL(&s_lock);
  const esp_err_t error = s_status.ready ? ESP_OK : ESP_ERR_INVALID_STATE;
  if (error == ESP_OK) {
    s_machine_command = true; s_machine_start = false;
    s_status.machine_active = s_status.machine_pending = false;
    s_status.machine_continuous = false;
    s_status.machine_remaining_seconds = 0;
  }
  portEXIT_CRITICAL(&s_lock);
  return error;
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
  if (s_machine_command) {
    out->machine_pending = s_machine_start;
    if (!s_machine_start) out->machine_active = false;
  }
  if (alarm.phase == WAC_ALARM_RINGING || out->preview_active || !out->ready)
    out->machine_active = out->machine_pending = false;
  if (!out->machine_active && !out->machine_pending) {
    out->machine_continuous = false;
    out->machine_remaining_seconds = 0;
  }
  if (alarm.phase == WAC_ALARM_RINGING || s_weather_requested || !out->ready) out->preview_active = false;
  portEXIT_CRITICAL(&s_lock);
}

esp_err_t wac_audio_weather_start(wac_audio_weather_kind_t kind, uint32_t *token)
{
  if (!token || (kind != WAC_AUDIO_WEATHER_WATCH && kind != WAC_AUDIO_WEATHER_WARNING))
    return ESP_ERR_INVALID_ARG;
  *token = 0;
  wac_alarm_snapshot_t alarm;
  wac_alarm_copy(&alarm);
  portENTER_CRITICAL(&s_lock);
  const esp_err_t error = !s_status.ready || alarm.phase == WAC_ALARM_RINGING ||
    s_status.weather_state != WAC_AUDIO_WEATHER_NONE ? ESP_ERR_INVALID_STATE : ESP_OK;
  if (error == ESP_OK) {
    if (!++s_weather_next_token) ++s_weather_next_token;
    *token = s_status.weather_token = s_weather_next_token;
    s_status.weather_kind = kind;
    s_status.weather_state = WAC_AUDIO_WEATHER_PENDING;
    s_status.weather_started_us = 0;
    s_status.weather_error = ESP_OK;
    s_status.preview_active = false;
    preview_command_t discarded;
    (void)xQueueReceive(s_commands, &discarded, 0);
    s_weather_requested = true;
    ++s_weather_generation;
  }
  portEXIT_CRITICAL(&s_lock);
  return error;
}

esp_err_t wac_audio_weather_stop(uint32_t token)
{
  portENTER_CRITICAL(&s_lock);
  const esp_err_t error = s_status.ready ? ESP_OK : ESP_ERR_INVALID_STATE;
  if (error == ESP_OK && token && token == s_status.weather_token && s_weather_requested) {
    s_weather_requested = false;
    ++s_weather_generation;
  }
  portEXIT_CRITICAL(&s_lock);
  return error;
}

void wac_audio_weather_ack(uint32_t token)
{
  portENTER_CRITICAL(&s_lock);
  if (token && token == s_status.weather_token && s_status.weather_state >= WAC_AUDIO_WEATHER_FINISHED) {
    s_status.weather_state = WAC_AUDIO_WEATHER_NONE;
    s_status.weather_token = 0;
    s_status.weather_started_us = 0;
    s_status.weather_error = ESP_OK;
  }
  portEXIT_CRITICAL(&s_lock);
}
