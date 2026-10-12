#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_support.h"
/* Execute the real private worker and generator without firmware test hooks. */
#include "../main/audio.c"

struct fake_queue { size_t size; bool pending; preview_command_t value; };
static struct fake_queue queue;
static bool queue_fail, task_fail, bsp_fail;
static esp_err_t open_error, volume_error, write_error, close_error;
static unsigned creates, opens, volumes, writes, closes, tasks, deleted;
static bool codec_open;
static void (*worker_function)(void *);
static pthread_t worker_thread;
static pthread_t caller_thread;
static pthread_mutex_t gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static unsigned boundary_count;
static bool permit, finish, at_write;
static bool pause_open;
static int64_t setup_delay;
static int64_t now;
static int16_t last_pcm[CHUNK_SAMPLES];
static wac_alarm_snapshot_t live_alarm;
static device_config_t applied = { .alarm_sound = DEVICE_CONFIG_ALARM_CLASSIC_BEEP, .alarm_volume = 50 };
static void boundary(bool writing);

QueueHandle_t xQueueCreate(UBaseType_t depth, UBaseType_t size)
{
  assert(depth == 1 && size == sizeof(preview_command_t));
  queue.size = size;
  return queue_fail ? NULL : &queue;
}
void vQueueDelete(QueueHandle_t handle) { assert(handle == &queue); ++deleted; }
BaseType_t xQueueOverwrite(QueueHandle_t handle, const void *value)
{
  assert(handle == &queue);
  memcpy(&queue.value, value, queue.size); queue.pending = true; return pdPASS;
}
BaseType_t xQueueReceive(QueueHandle_t handle, void *value, TickType_t wait)
{
  assert(handle == &queue && wait == 0);
  if (!queue.pending) return pdFALSE;
  memcpy(value, &queue.value, queue.size); queue.pending = false; return pdTRUE;
}
BaseType_t xQueuePeek(QueueHandle_t handle, void *value, TickType_t wait)
{
  assert(handle == &queue && wait == 0);
  if (!queue.pending) return pdFALSE;
  memcpy(value, &queue.value, queue.size); return pdTRUE;
}
BaseType_t xTaskCreate(void (*fn)(void *), const char *name, uint32_t stack,
                       void *arg, UBaseType_t priority, TaskHandle_t *handle)
{
  assert(!strcmp(name, "audio") && stack > 0 && !arg && priority == 4 && !handle);
  ++tasks; worker_function = fn; return task_fail ? pdFALSE : pdPASS;
}
static void hardware_thread(void)
{
  assert(!pthread_equal(pthread_self(), caller_thread));
  assert(pthread_mutex_trylock(&s_lock) == 0);
  pthread_mutex_unlock(&s_lock);
}
esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void)
{
  hardware_thread(); ++creates; return bsp_fail ? NULL : &codec_open;
}
int esp_codec_dev_open(esp_codec_dev_handle_t handle, esp_codec_dev_sample_info_t *format)
{
  hardware_thread(); assert(handle == &codec_open && !codec_open);
  assert(format->bits_per_sample == 16 && format->channel == 1 && format->sample_rate == 22050);
  assert(format->channel_mask == 0 && format->mclk_multiple == 0);
  ++opens; codec_open = true;
  now += setup_delay; setup_delay = 0;
  if (pause_open) { pause_open = false; boundary(false); }
  return open_error;
}
int esp_codec_dev_set_out_vol(esp_codec_dev_handle_t handle, int volume)
{
  hardware_thread(); assert(handle == &codec_open && codec_open && volume == 100);
  ++volumes; return volume_error;
}
static void boundary(bool writing)
{
  pthread_mutex_lock(&gate);
  at_write = writing; ++boundary_count;
  pthread_cond_broadcast(&changed);
  while (!permit && !finish) pthread_cond_wait(&changed, &gate);
  permit = false;
  if (finish) { pthread_mutex_unlock(&gate); pthread_exit(NULL); }
  pthread_mutex_unlock(&gate);
}
int esp_codec_dev_write(esp_codec_dev_handle_t handle, void *data, int bytes)
{
  hardware_thread(); assert(handle == &codec_open && codec_open && bytes == (int)sizeof(last_pcm));
  memcpy(last_pcm, data, sizeof(last_pcm)); ++writes; now += 20000;
  boundary(true); return write_error;
}
int esp_codec_dev_close(esp_codec_dev_handle_t handle)
{
  hardware_thread(); assert(handle == &codec_open && codec_open);
  ++closes; codec_open = false; return close_error;
}
void vTaskDelay(TickType_t ticks)
{
  hardware_thread(); assert(ticks == 20); now += (int64_t)ticks * 1000; boundary(false);
}
int64_t esp_timer_get_time(void) { return now; }
void wac_alarm_copy(wac_alarm_snapshot_t *out)
{
  pthread_mutex_lock(&gate); *out = live_alarm; pthread_mutex_unlock(&gate);
}
esp_err_t device_config_copy(device_config_t *out)
{
  pthread_mutex_lock(&gate); *out = applied; pthread_mutex_unlock(&gate); return ESP_OK;
}
static void *run_worker(void *arg) { worker_function(arg); return NULL; }
static void start_worker(void)
{
  assert(pthread_create(&worker_thread, NULL, run_worker, NULL) == 0);
  pthread_mutex_lock(&gate);
  while (!boundary_count) pthread_cond_wait(&changed, &gate);
  pthread_mutex_unlock(&gate);
}
static void step(void)
{
  pthread_mutex_lock(&gate);
  unsigned before = boundary_count;
  permit = true; pthread_cond_broadcast(&changed);
  while (boundary_count == before) pthread_cond_wait(&changed, &gate);
  pthread_mutex_unlock(&gate);
}
static void stop_worker(void)
{
  pthread_mutex_lock(&gate); finish = true; pthread_cond_broadcast(&changed); pthread_mutex_unlock(&gate);
  assert(pthread_join(worker_thread, NULL) == 0);
}
static void alarm_phase(wac_alarm_phase_t phase, time_t utc)
{
  pthread_mutex_lock(&gate);
  live_alarm.phase = phase;
  live_alarm.current = (wac_alarm_occurrence_t){.valid = true, .utc = utc, .local_date = 20261011, .minute = 420};
  pthread_mutex_unlock(&gate);
}
static wac_audio_snapshot_t snapshot(void)
{
  wac_audio_snapshot_t result; wac_audio_copy(&result); return result;
}
static unsigned peak(const int16_t *pcm)
{
  unsigned result = 0;
  for (unsigned i = 0; i < CHUNK_SAMPLES; ++i) {
    unsigned value = (unsigned)abs(pcm[i]); if (value > result) result = value;
  }
  return result;
}
static double spectral(device_config_alarm_sound_t sound, double hz)
{
  double real = 0, imaginary = 0;
  for (unsigned i = 0; i < 5000; ++i) {
    double t = (double)i / SAMPLE_RATE;
    double value = sound_sample(sound, i, true);
    const double angle = 2 * 3.14159265358979323846 * hz * t;
    real += value * cos(angle); imaginary += value * sin(angle);
  }
  return hypot(real, imaginary);
}
/* The original double-precision equations provide an independent numerical
 * reference. They are deliberately kept out of the firmware's hot path. */
static double reference_tone(double time, double offset, double duration,
                             double frequency, double level, unsigned shape,
                             double limit)
{
  duration = fmin(duration, limit - offset);
  const double age = time - offset;
  if (age < 0 || age >= duration || duration <= 0) return 0;
  const double attack = fmin(.012, duration / 3);
  double envelope = age < attack ? level * age / attack :
                    level * fmin(1, (duration - age) / .02);
  const double wave = sin(2 * 3.14159265358979323846 * frequency * age);
  return envelope * (shape == 1 ? (wave >= 0 ? 1 : -1) :
                     shape == 2 ? 2 / 3.14159265358979323846 * asin(wave) : wave);
}
static double reference_sample(int sound, uint64_t frame, bool ringing)
{
  const double time = (double)frame / SAMPLE_RATE;
  const double period = sound == DEVICE_CONFIG_ALARM_CLASSIC_BEEP ? 1.1 : 1.4;
  const double base = floor(time / period) * period;
  const double limit = ringing ? INFINITY : 5;
  if (sound == DEVICE_CONFIG_ALARM_CLASSIC_BEEP)
    return reference_tone(time, base, .18, 880, .42, 1, limit) +
           reference_tone(time, base + .36, .18, 880, .42, 1, limit);
  double value = 0;
  const double rising[] = {440, 554.37, 659.25, 880};
  for (unsigned i = 0; i < 4; ++i)
    value += reference_tone(time, base + i * .24, .18, rising[i], .65, 2, limit);
  return value;
}
static bool classic_zero_crossing(uint64_t frame)
{
  unsigned local = frame % 24255;
  if (local >= 7938 && local < 7938 + 3969) local -= 7938;
  return local < 3969 && (local * 1760) % SAMPLE_RATE == 0;
}
static void precision_checks(void)
{
  const unsigned volumes_to_check[] = {0, 7, 37, 100};
  const float ramp_seconds[] = {0, 15, 30};
  const uint32_t periods[] = {24255, 0, 30870};
  int16_t pcm[CHUNK_SAMPLES], later[CHUNK_SAMPLES];
  bell_state_t bell = {0};
  for (int sound = 0; sound < 3; sound += 2) {
    unsigned max_difference = 0, crossings = 0;
    for (unsigned mode = 0; mode < 4; ++mode) {
      bool ringing = mode != 0;
      float seconds = ringing ? ramp_seconds[mode - 1] : 0;
      double ramp = ringing ? .1 + .9 * fmin(1, seconds / 30) : 1;
      for (unsigned v = 0; v < 4; ++v) {
        unsigned changed_crossings = 0;
        for (unsigned chunk = 0; chunk < 250; ++chunk) {
          uint64_t frame = chunk * CHUNK_SAMPLES;
          fill_chunk(pcm, frame, sound, volumes_to_check[v], ringing, seconds, &bell);
          for (unsigned i = 0; i < CHUNK_SAMPLES; ++i) {
            double expected = reference_sample(sound, frame + i, ringing) *
                              volumes_to_check[v] * volumes_to_check[v] / 10000.0 * .82 * ramp;
            int reference = (int)lrint(fmax(-1, fmin(1, expected)) * INT16_MAX);
            unsigned difference = (unsigned)abs(pcm[i] - reference);
            /* A square has no unique sign at an exact zero of its sine.
             * The old double sine's roundoff chose either sign there. Permit
             * only these isolated exact crossings, never adjacent samples. */
            if (sound == 0 && difference > 2 && classic_zero_crossing(frame + i)) {
              ++changed_crossings;
              continue;
            }
            if (difference > max_difference) max_difference = difference;
            assert(difference <= 2); /* 2/32767 = 0.0062% of full-scale PCM. */
          }
        }
        assert(changed_crossings <= 10);
        if (changed_crossings > crossings) crossings = changed_crossings;
      }
    }
    /* Hours/days of ringing and a chunk crossing its period boundary must
     * remain sample-identical; an ever-growing float clock fails this check. */
    const uint64_t repeats[] = {4000, 100000, 1000000};
    const uint32_t positions[] = {0, 100, periods[sound] - CHUNK_SAMPLES / 2};
    for (unsigned r = 0; r < 3; ++r)
      for (unsigned p = 0; p < 3; ++p) {
        fill_chunk(pcm, positions[p], sound, 37, true, 30, &bell);
        fill_chunk(later, repeats[r] * periods[sound] + positions[p], sound, 37, true, 30, &bell);
        assert(memcmp(pcm, later, sizeof(pcm)) == 0);
      }
    printf("sound %d reference max error %u PCM; exact square crossings %u\n",
           sound, max_difference, crossings);
  }
}
/* Frequency checks look for the selected metallic modes and two pitches,
 * rather than reproducing the generator equations or its random stream. */
static double bell_band(const float *samples, unsigned start, unsigned count, double hz)
{
  double real = 0, imaginary = 0;
  for (unsigned i = 0; i < count; ++i) {
    double angle = 2 * 3.14159265358979323846 * hz * i / SAMPLE_RATE;
    double window = .5 - .5 * cos(2 * 3.14159265358979323846 * i / (count - 1));
    real += samples[start + i] * window * cos(angle);
    imaginary += samples[start + i] * window * sin(angle);
  }
  return hypot(real, imaginary);
}
static double energy(const float *samples, unsigned start, unsigned count)
{
  double sum = 0;
  for (unsigned i = 0; i < count; ++i) sum += samples[start + i] * samples[start + i];
  return sum / count;
}
static void fresh_chunk(int16_t *pcm, uint64_t frame, device_config_alarm_sound_t sound,
                        uint8_t volume, bool ringing, float seconds)
{
  bell_state_t bell;
  bell_reset(&bell);
  if (sound == DEVICE_CONFIG_ALARM_CLASSIC_BELL)
    for (uint64_t i = 0; i < frame; ++i) (void)bell_sample(&bell);
  fill_chunk(pcm, frame, sound, volume, ringing, seconds, &bell);
}
static void bell_character_checks(void)
{
  enum { STRIKE = 1050, CAPTURE = SAMPLE_RATE * 2 };
  float samples[CAPTURE];
  bell_state_t bell;
  bell_reset(&bell);
  for (unsigned i = 0; i < CAPTURE; ++i) samples[i] = bell_sample(&bell);
  /* A fast hammer attack reaches useful energy within 1.5ms. The retired
   * 12ms melody attack would fail this early/settled energy comparison. */
  assert(energy(samples, 0, 32) > .2 * energy(samples, 32, 128));
  const double modes[] = {2440, 3370, 4690};
  const double between[] = {2200, 3000, 4300};
  for (unsigned i = 0; i < 3; ++i)
    assert(bell_band(samples, 64, 900, modes[i]) > 4 * bell_band(samples, 64, 900, between[i]));
  const double low = bell_band(samples, 64, 900, 3370);
  const double high = bell_band(samples, STRIKE + 64, 900, 3370 * 1.061);
  assert(high > 3 * bell_band(samples, 64, 900, 3370 * 1.061));
  /* The first bell is still audible while the other bell is struck. This
   * rejects an implementation that replaces the current tail on every hit. */
  assert(bell_band(samples, STRIKE + 64, 900, 3370) > .12 * low);
  assert(high > .12 * low);
  /* The bright, quickly fading upper mode exposes each hammer arrival.
   * Check both pitches throughout two seconds at 21 total strikes/second,
   * allowing 100 samples of light clatter variation around each strike. */
  for (unsigned strike = 1; strike < 42; ++strike) {
    const unsigned at = strike * STRIKE;
    const double hz = 9560 * (strike % 2 ? 1.061 : 1);
    assert(bell_band(samples, at - 100, 400, hz) >
           2 * bell_band(samples, at - 550, 300, hz));
  }
  /* Chunk boundaries, including a chunk containing a new strike, retain the
   * same tails as individual sequential samples. */
  int16_t pcm[CHUNK_SAMPLES];
  bell_reset(&bell);
  for (unsigned chunk = 0; chunk < 8; ++chunk) {
    fill_chunk(pcm, chunk * CHUNK_SAMPLES, DEVICE_CONFIG_ALARM_CLASSIC_BELL,
               100, false, 0, &bell);
    for (unsigned i = 0; i < CHUNK_SAMPLES; ++i)
      assert(abs(pcm[i] - (int)lrint(samples[chunk * CHUNK_SAMPLES + i] * .82 * INT16_MAX)) <= 1);
  }
  /* Start with an already ringing state, then move only its whole-sample
   * position forward by over four days. Neither oscillators nor strike
   * boundaries may start using a lossy ever-growing floating-point clock. */
  bell_state_t distant = bell;
  const uint64_t shift = UINT64_C(8400) * 1000000;
  distant.next_frame += shift;
  int16_t later[CHUNK_SAMPLES];
  for (unsigned chunk = 0; chunk < 20; ++chunk) {
    uint64_t frame = bell.next_frame;
    fill_chunk(pcm, frame, DEVICE_CONFIG_ALARM_CLASSIC_BELL, 100, true, 30, &bell);
    fill_chunk(later, frame + shift, DEVICE_CONFIG_ALARM_CLASSIC_BELL, 100, true, 30, &distant);
    assert(memcmp(pcm, later, sizeof(pcm)) == 0);
  }
  printf("Classic Bell: fast attack, alternating metallic pitches, 21 strikes/s and overlapping tails passed\n");
}
static void generator_checks(void)
{
  assert(sound_sample(DEVICE_CONFIG_ALARM_CLASSIC_BEEP, 4410, true) == 0);
  assert(sound_sample(DEVICE_CONFIG_ALARM_CLASSIC_BEEP, 13230, true) == 0);
  assert(fabs(sound_sample(DEVICE_CONFIG_ALARM_CLASSIC_BEEP, 8379, true)) > .4);
  assert(fabs(sound_sample(DEVICE_CONFIG_ALARM_CLASSIC_BEEP, 24696, true)) > .4);
  assert(fabs(tone(3969, 0, .18f, 880, .42f, 1, UINT32_MAX)) == 0);
  assert(spectral(DEVICE_CONFIG_ALARM_CLASSIC_BEEP, 880) > 10 * spectral(DEVICE_CONFIG_ALARM_CLASSIC_BEEP, 700));
  assert(spectral(DEVICE_CONFIG_ALARM_RISING_TONES, 440) > 10 * spectral(DEVICE_CONFIG_ALARM_RISING_TONES, 523.25));
  precision_checks();
  bell_character_checks();
  int16_t pcm[CHUNK_SAMPLES], full[CHUNK_SAMPLES];
  const unsigned volume_values[] = {0, 7, 37, 50, 100};
  const float seconds_values[] = {0, 15, 30};
  for (int sound = 0; sound < 3; ++sound) {
    unsigned loudest = 0;
    for (unsigned ringing = 0; ringing < 2; ++ringing) {
      bell_state_t bell;
      bell_reset(&bell);
      for (unsigned chunk = 0; chunk < 250; ++chunk) {
        fill_chunk(pcm, chunk * CHUNK_SAMPLES, sound, 100, ringing, 30, &bell);
        unsigned chunk_peak = peak(pcm);
        if (chunk_peak > loudest) loudest = chunk_peak;
        assert(chunk_peak < INT16_MAX); /* No clipped full-volume preview or ring. */
        if (sound == DEVICE_CONFIG_ALARM_CLASSIC_BELL)
          assert(chunk_peak < 21800); /* Keeps the existing codec amplification headroom. */
      }
    }
    if (sound == DEVICE_CONFIG_ALARM_CLASSIC_BELL) assert(loudest > 16000);
    fresh_chunk(full, 882, sound, 100, false, 0);
    for (unsigned v = 0; v < sizeof(volume_values) / sizeof(volume_values[0]); ++v) {
      unsigned volume = volume_values[v];
      fresh_chunk(pcm, 882, sound, volume, false, 0);
      for (unsigned i = 0; i < CHUNK_SAMPLES; ++i)
        assert(fabs(pcm[i] - (double)full[i] * volume * volume / 10000.0) <= 1);
      for (unsigned r = 0; r < 3; ++r) {
        double ramp = .1 + .9 * seconds_values[r] / 30;
        fresh_chunk(pcm, 882, sound, volume, true, seconds_values[r]);
        for (unsigned i = 0; i < CHUNK_SAMPLES; ++i)
          assert(fabs(pcm[i] - (double)full[i] * volume * volume / 10000.0 * ramp) <= 1);
      }
    }
    fresh_chunk(pcm, 882, sound, 100, true, 100);
    assert(memcmp(pcm, full, sizeof(pcm)) == 0);
    fresh_chunk(pcm, 5 * SAMPLE_RATE, sound, 100, false, 0);
    assert(peak(pcm) == 0);
  }
}

static void playback_checks(void)
{
  setup_delay = 2000000;
  assert(wac_audio_preview(DEVICE_CONFIG_ALARM_CLASSIC_BELL, 37) == ESP_OK);
  assert(snapshot().preview_active); step(); assert(at_write && creates == 1 && opens == 1);
  unsigned started_writes = writes;
  while (at_write) step();
  assert(writes - started_writes + 1 == 250 && !snapshot().preview_active && closes == 1);
  int16_t first_bell[CHUNK_SAMPLES], expected[CHUNK_SAMPLES];
  assert(wac_audio_preview(DEVICE_CONFIG_ALARM_CLASSIC_BELL, 37) == ESP_OK);
  step(); assert(at_write);
  fresh_chunk(first_bell, 0, DEVICE_CONFIG_ALARM_CLASSIC_BELL, 37, false, 0);
  assert(memcmp(first_bell, last_pcm, sizeof(first_bell)) == 0);
  for (unsigned i = 0; i < 8; ++i) step(); /* Build up overlapping tails. */
  assert(wac_audio_preview(DEVICE_CONFIG_ALARM_CLASSIC_BELL, 37) == ESP_OK);
  step(); assert(memcmp(first_bell, last_pcm, sizeof(first_bell)) == 0);
  assert(wac_audio_preview(DEVICE_CONFIG_ALARM_RISING_TONES, 37) == ESP_OK);
  step(); fresh_chunk(expected, 0, DEVICE_CONFIG_ALARM_RISING_TONES, 37, false, 0);
  assert(memcmp(expected, last_pcm, sizeof(expected)) == 0);
  assert(wac_audio_preview(DEVICE_CONFIG_ALARM_CLASSIC_BELL, 37) == ESP_OK);
  step(); assert(memcmp(first_bell, last_pcm, sizeof(first_bell)) == 0);
  assert(wac_audio_stop_preview() == ESP_OK); step(); assert(!at_write);
  assert(wac_audio_preview(DEVICE_CONFIG_ALARM_CLASSIC_BELL, 7) == ESP_OK);
  assert(wac_audio_preview(DEVICE_CONFIG_ALARM_CLASSIC_BEEP, 37) == ESP_OK);
  step(); assert(at_write);
  fresh_chunk(expected, 0, DEVICE_CONFIG_ALARM_CLASSIC_BEEP, 37, false, 0);
  assert(memcmp(expected, last_pcm, sizeof(expected)) == 0);
  /* Main can submit cancellation while the worker is inside its write. */
  assert(wac_audio_stop_preview() == ESP_OK && !snapshot().preview_active);
  unsigned before = writes; step(); assert(!at_write && writes == before && !codec_open);
  assert(wac_audio_preview(DEVICE_CONFIG_ALARM_CLASSIC_BELL, 50) == ESP_OK);
  pthread_mutex_lock(&gate); applied.alarm_sound = DEVICE_CONFIG_ALARM_CLASSIC_BELL; pthread_mutex_unlock(&gate);
  alarm_phase(WAC_ALARM_RINGING, 100); assert(!snapshot().preview_active);
  step(); assert(at_write && !snapshot().preview_active);
  fresh_chunk(expected, 0, applied.alarm_sound, applied.alarm_volume, true, 0);
  assert(memcmp(expected, last_pcm, sizeof(expected)) == 0);
  assert(wac_audio_preview(DEVICE_CONFIG_ALARM_RISING_TONES, 100) == ESP_ERR_INVALID_STATE);
  step(); assert(at_write && !snapshot().preview_active);
  /* The alarm owner has published Snooze/Dismiss before a storage operation
   * completes; the worker only reads that snapshot, never its storage result. */
  alarm_phase(WAC_ALARM_SNOOZED, 100); before = writes; step();
  assert(!at_write && writes == before && !codec_open);
  step(); assert(!at_write && !snapshot().preview_active);
  alarm_phase(WAC_ALARM_RINGING, 100); step();
  assert(memcmp(expected, last_pcm, sizeof(expected)) == 0);
  alarm_phase(WAC_ALARM_RINGING, 101); step();
  assert(memcmp(expected, last_pcm, sizeof(expected)) == 0);
  alarm_phase(WAC_ALARM_IDLE, 101); step(); assert(!codec_open);
  assert(wac_audio_preview(DEVICE_CONFIG_ALARM_CLASSIC_BELL, 0) == ESP_OK);
  step(); assert(at_write && peak(last_pcm) == 0);
  assert(wac_audio_stop_preview() == ESP_OK); step();
  assert(creates == 1 && !codec_open);
  /* A cancellation or alarm arriving during slow initial/reopen setup is
   * checked before even the first PCM write. */
  pause_open = true;
  assert(wac_audio_preview(DEVICE_CONFIG_ALARM_CLASSIC_BEEP, 50) == ESP_OK);
  before = writes; step(); assert(codec_open && !at_write && writes == before);
  assert(wac_audio_stop_preview() == ESP_OK); step();
  assert(!codec_open && writes == before);
  pause_open = true;
  assert(wac_audio_preview(DEVICE_CONFIG_ALARM_CLASSIC_BELL, 50) == ESP_OK);
  step(); alarm_phase(WAC_ALARM_RINGING, 102); step();
  assert(at_write && writes == before + 1);
  assert(memcmp(expected, last_pcm, sizeof(expected)) == 0);
  alarm_phase(WAC_ALARM_IDLE, 102); step(); assert(!codec_open);
}

int main(int argc, char **argv)
{
  caller_thread = pthread_self();
  assert(argc == 2);
  const char *scenario = argv[1];
  if (!strcmp(scenario, "generator")) { generator_checks(); puts("audio generator passed"); return 0; }
  assert(wac_audio_preview(DEVICE_CONFIG_ALARM_CLASSIC_BEEP, 50) == ESP_ERR_INVALID_STATE);
  assert(wac_audio_stop_preview() == ESP_ERR_INVALID_STATE);
  assert(wac_audio_preview(99, 50) == ESP_ERR_INVALID_ARG);
  assert(wac_audio_preview(DEVICE_CONFIG_ALARM_CLASSIC_BEEP, 101) == ESP_ERR_INVALID_ARG);
  queue_fail = !strcmp(scenario, "queuefail"); task_fail = !strcmp(scenario, "taskfail");
  if (queue_fail || task_fail) {
    assert(wac_audio_init() == ESP_ERR_NO_MEM && !snapshot().ready);
    assert(wac_audio_init() == ESP_ERR_NO_MEM);
    assert(wac_audio_preview(DEVICE_CONFIG_ALARM_CLASSIC_BEEP, 50) == ESP_ERR_INVALID_STATE);
    assert(tasks == (unsigned)task_fail && deleted == (unsigned)task_fail);
    return 0;
  }
  bsp_fail = !strcmp(scenario, "bspfail");
  open_error = !strcmp(scenario, "openfail") ? ESP_FAIL : ESP_OK;
  volume_error = !strcmp(scenario, "volumefail") ? ESP_FAIL : ESP_OK;
  write_error = !strcmp(scenario, "writefail") ? ESP_FAIL : ESP_OK;
  close_error = !strcmp(scenario, "closefail") ? ESP_FAIL : ESP_OK;
  assert(wac_audio_init() == ESP_OK && wac_audio_init() == ESP_OK && tasks == 1);
  start_worker(); assert(creates == 0 && !at_write);
  if (!strcmp(scenario, "playback")) playback_checks();
  else {
    alarm_phase(WAC_ALARM_RINGING, 100); step();
    if (write_error) { assert(at_write); step(); }
    if (close_error) { assert(at_write); alarm_phase(WAC_ALARM_IDLE, 100); step(); }
    assert(!at_write && !codec_open && !snapshot().preview_active);
    assert(snapshot().error == (bsp_fail ? ESP_ERR_INVALID_STATE : ESP_FAIL));
    assert(snapshot().ready != bsp_fail);
    const unsigned attempts = opens;
    for (unsigned i = 0; i < 20; ++i) step();
    assert(creates == 1 && opens == attempts);
    if (bsp_fail) assert(wac_audio_preview(DEVICE_CONFIG_ALARM_CLASSIC_BEEP, 50) == ESP_ERR_INVALID_STATE);
    else {
      assert(closes == 1);
      open_error = volume_error = write_error = close_error = ESP_OK;
      alarm_phase(WAC_ALARM_IDLE, 100); step();
      assert(wac_audio_preview(DEVICE_CONFIG_ALARM_RISING_TONES, 50) == ESP_OK);
      step(); assert(at_write && snapshot().error == ESP_OK && snapshot().preview_active);
      assert(wac_audio_stop_preview() == ESP_OK); step(); assert(!codec_open);
    }
  }
  stop_worker(); printf("audio %s passed\n", scenario); return 0;
}
