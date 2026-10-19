#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "test_support.h"
static void *audio_malloc(size_t size);
static void audio_free(void *memory);
#define malloc audio_malloc
#define free audio_free
static FILE *audio_fopen(const char *path, const char *mode);
static int audio_fclose(FILE *file);
static size_t audio_fread(void *data, size_t size, size_t count, FILE *file);
#define fread audio_fread
esp_err_t bsp_spiffs_mount(void);
#define fopen audio_fopen
#define fclose audio_fclose
/* Execute the real private worker and generator without firmware test hooks. */
#include "../main/audio.c"
#undef fopen
#undef fclose
#undef fread
#undef malloc
#undef free

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
static bool pause_open, pause_creation, pause_volume;
static int64_t setup_delay;
static int64_t now;
static int16_t last_pcm[MACHINE_MAX_SAMPLES];
static unsigned last_count, codec_rate;
static bool machine_scenario, guard_pcm_fixture;
static esp_err_t mount_error;
static esp_audio_err_t register_error, decoder_open_error, info_error;
static bool missing_file, bad_metadata, read_error;
static unsigned allocation_attempts, allocations_live, allocation_fail_at;
static unsigned mounts, decoder_opens, decoder_closes, decoder_resets, decoder_calls;
static bool decode_fail, decode_stall, format_fail, reset_fail, decode_short, buffer_retry, oversized;
static bool pause_mount, pause_decode, pause_decoder_open, pause_read;
static unsigned read_calls, fail_read_at;
static unsigned file_opens, file_closes, fail_file_open_at, fail_file_close_at;
static FILE *recording_file;
static bool pause_file_open;
static unsigned loops_verified;
static bool recovery_fail;
static wac_alarm_snapshot_t live_alarm;
static device_config_t applied = { .alarm_sound = DEVICE_CONFIG_ALARM_CLASSIC_BEEP, .alarm_volume = 50,
  .sound_machine_sound = DEVICE_CONFIG_SOUND_MACHINE_WHITE_NOISE,
  .sound_machine_volume = 30, .sound_machine_timer_minutes = 30 };
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
BaseType_t xTaskCreatePinnedToCore(TaskFunction_t fn, const char * const name,
                                  const configSTACK_DEPTH_TYPE stack, void * const arg,
                                  UBaseType_t priority, TaskHandle_t * const handle,
                                  const BaseType_t core)
{
  assert(!strcmp(name, "audio") && stack > 0 && !arg && priority == 4 && !handle && core == 1);
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
  hardware_thread(); ++creates;
  if (pause_creation) { pause_creation = false; boundary(false); }
  return bsp_fail ? NULL : &codec_open;
}
int esp_codec_dev_open(esp_codec_dev_handle_t handle, esp_codec_dev_sample_info_t *format)
{
  hardware_thread(); assert(handle == &codec_open && !codec_open);
  assert(format->bits_per_sample == 16 && format->channel == 1);
  assert(format->sample_rate == 22050 || (machine_scenario &&
         (format->sample_rate == 24000 || format->sample_rate == 44100)));
  codec_rate = format->sample_rate;
  assert(format->channel_mask == 0 && format->mclk_multiple == 0);
  ++opens; codec_open = true;
  now += setup_delay; setup_delay = 0;
  if (pause_open) { pause_open = false; boundary(false); }
  return open_error;
}
int esp_codec_dev_set_out_vol(esp_codec_dev_handle_t handle, int volume)
{
  hardware_thread(); assert(handle == &codec_open && codec_open && volume == 100);
  ++volumes;
  if (pause_volume) { pause_volume = false; boundary(false); }
  return volume_error;
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
  hardware_thread(); assert(handle == &codec_open && codec_open);
  assert(bytes == (int)(codec_rate / 50 * sizeof(int16_t)));
  if (codec_rate == 22050) assert(bytes == CHUNK_SAMPLES * (int)sizeof(int16_t));
  last_count = bytes / sizeof(int16_t);
  memcpy(last_pcm, data, bytes); ++writes; now += 20000;
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
/* Simulated decoder: validates real encoded-frame boundaries but emits a
 * deterministic PCM sequence, not decoded sound. Actual library/board behavior
 * requires target verification. Actual managed headers establish ABI signatures. */
typedef struct {
  uint32_t rate;
  uint64_t total, emitted;
  unsigned leading;
  bool metadata;
} fake_decoder_t;
static size_t encoded_frame_size(const uint8_t *data, size_t size, unsigned *samples)
{
  if (size < 4) return 0;
  const uint32_t h = be32(data);
  const unsigned version = (h >> 19) & 3, index = (h >> 12) & 15, frequency = (h >> 10) & 3;
  if (version == 1 || frequency == 3 || index == 0 || index == 15) return 0;
  static const unsigned rate[] = {44100, 48000, 32000};
  static const unsigned b1[] = {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320};
  static const unsigned b2[] = {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160};
  *samples = version == 3 ? 1152 : 576;
  return ((version == 3 ? 144000 * b1[index] : 72000 * b2[index]) /
          (rate[frequency] / (version == 3 ? 1 : 2))) + ((h >> 9) & 1);
}
esp_err_t bsp_spiffs_mount(void)
{
  hardware_thread(); ++mounts;
  if (pause_mount) { pause_mount = false; boundary(false); }
  return mount_error;
}
static void *audio_malloc(size_t size)
{
  hardware_thread(); ++allocation_attempts;
  if (allocation_fail_at && allocation_attempts == allocation_fail_at) return NULL;
  void *memory = malloc(size); assert(memory); ++allocations_live; return memory;
}
static void audio_free(void *memory)
{
  if (memory) { hardware_thread(); assert(allocations_live); --allocations_live; free(memory); }
}
static FILE *audio_fopen(const char *path, const char *mode)
{
  hardware_thread(); ++file_opens;
  assert(!strncmp(path, "/spiffs/", 8));
  assert(!recording_file);
  if (missing_file || file_opens == fail_file_open_at) return NULL;
  FILE *file;
  if (bad_metadata) {
    file = tmpfile(); assert(file);
    assert(fwrite("broken", 1, 6, file) == 6); rewind(file);
  } else {
    char actual[1024];
    snprintf(actual, sizeof(actual), "%s/%s", WAC_AUDIO_ASSET_DIR, path + 8);
    file = fopen(actual, mode);
  }
  recording_file = file;
  if (read_error && file) assert(close(fileno(file)) == 0);
  if (pause_file_open) { pause_file_open = false; boundary(false); }
  return file;
}
static int audio_fclose(FILE *file)
{
  hardware_thread(); assert(file && file == recording_file);
  recording_file = NULL; ++file_closes;
  const int result = fclose(file);
  return file_closes == fail_file_close_at ? EOF : result;
}
static size_t audio_fread(void *data, size_t size, size_t count, FILE *file)
{
  hardware_thread(); ++read_calls;
  if (pause_read) { pause_read = false; boundary(false); }
  if (fail_read_at && read_calls == fail_read_at) assert(close(fileno(file)) == 0);
  return fread(data, size, count, file);
}
esp_audio_err_t esp_mp3_dec_register(void) { hardware_thread(); return register_error; }
esp_audio_err_t esp_audio_simple_dec_open(esp_audio_simple_dec_cfg_t *cfg, esp_audio_simple_dec_handle_t *out)
{
  hardware_thread();
  assert(cfg->dec_type == ESP_AUDIO_SIMPLE_DEC_TYPE_MP3 && !cfg->dec_cfg && !cfg->cfg_size && !cfg->use_frame_dec);
  if (decoder_open_error) return decoder_open_error;
  *out = calloc(1, sizeof(fake_decoder_t)); assert(*out); ++decoder_opens;
  if (pause_decoder_open) { pause_decoder_open = false; boundary(false); }
  return ESP_AUDIO_ERR_OK;
}
esp_audio_err_t esp_audio_simple_dec_process(esp_audio_simple_dec_handle_t handle,
                                            esp_audio_simple_dec_raw_t *raw, esp_audio_simple_dec_out_t *output)
{
  hardware_thread(); ++decoder_calls;
  if (recovery_fail) raw->frame_recover = ESP_AUDIO_SIMPLE_DEC_RECOVERY_PLC;
  fake_decoder_t *decoder = handle;
  assert(raw->consumed == 0 && output->decoded_size == 0 && output->len == MP3_OUTPUT_BYTES);
  if (pause_decode) { pause_decode = false; boundary(false); }
  if (decode_fail) return ESP_AUDIO_ERR_FAIL;
  if (decode_stall) return ESP_AUDIO_ERR_OK;
  if (buffer_retry || oversized) {
    buffer_retry = false;
    output->needed_size = oversized ? MP3_OUTPUT_BYTES + 1 : MP3_OUTPUT_BYTES;
    return ESP_AUDIO_ERR_BUFF_NOT_ENOUGH;
  }
  unsigned samples;
  const size_t bytes = encoded_frame_size(raw->buffer, raw->len, &samples);
  if (!bytes || bytes > raw->len) return ESP_AUDIO_ERR_FAIL;
  raw->consumed = bytes;
  if (!decoder->metadata) {
    assert(recording_metadata(raw->buffer, bytes, &decoder->rate, &decoder->total));
    decoder->metadata = true;
    decoder->leading = 1105;
    return ESP_AUDIO_ERR_OK;
  }
  const unsigned trim = decoder->leading < samples ? decoder->leading : samples;
  decoder->leading -= trim; samples -= trim;
  if (decode_short) { output->decoded_size = 0; return ESP_AUDIO_ERR_OK; }
  /* Model the inspected MPEG2 tail discrepancy, so the application must cap
   * output at metadata N. MPEG1 automatic end cap remains represented. */
  if (decoder->rate == 44100 && samples > decoder->total - decoder->emitted)
    samples = decoder->total - decoder->emitted;
  int16_t *pcm = (int16_t *)output->buffer;
  for (unsigned i = 0; i < samples; ++i) {
    if (guard_pcm_fixture) {
      /* Constructed PCM checks both signs beyond the boosted output range,
       * plus quieter samples that must retain their level instead of clip. */
      static const int16_t levels[] = {12000, -12000, 3000, -3000};
      pcm[i] = levels[(decoder->emitted + i) % 4];
    } else pcm[i] = 1 + ((decoder->emitted + i) % 1000);
  }
  decoder->emitted += samples;
  output->decoded_size = samples * sizeof(int16_t);
  return ESP_AUDIO_ERR_OK;
}
esp_audio_err_t esp_audio_simple_dec_get_info(esp_audio_simple_dec_handle_t handle, esp_audio_simple_dec_info_t *info)
{
  hardware_thread(); fake_decoder_t *decoder = handle;
  if (info_error) return info_error;
  *info = (esp_audio_simple_dec_info_t){ .sample_rate = decoder->rate, .channel = format_fail ? 2 : 1,
                                       .bits_per_sample = 16 };
  return ESP_AUDIO_ERR_OK;
}
esp_audio_err_t esp_audio_simple_dec_reset(esp_audio_simple_dec_handle_t handle)
{
  hardware_thread(); fake_decoder_t *decoder = handle;
  ++decoder_resets;
  if (reset_fail) return ESP_AUDIO_ERR_FAIL;
  assert(decoder->emitted >= decoder->total);
  ++loops_verified;
  *decoder = (fake_decoder_t){0}; return ESP_AUDIO_ERR_OK;
}
void esp_audio_simple_dec_close(esp_audio_simple_dec_handle_t handle)
{
  hardware_thread(); ++decoder_closes; free(handle);
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

static void noise_checks(void)
{
  /* Frozen double-precision results from soundmachine.app/playback.js noiseBuffer
   * (SHA256 98952534416c94147e399652293764cdf28979723a5182ab570aee537ed76d3c),
   * supplying our existing xorshift's uniform input instead of Math.random.
   * These ordinary seed-1977 samples include a chunk boundary and natural Pink
   * and Brown excursions past the removed .65 limit. No production filter code
   * or coefficient table is used to calculate expected results at test time.
   * 1e-5 allows float coefficients/accumulation versus JavaScript doubles;
   * it is far below the retired clipping and altered Brown-equation errors. */
  static const struct { unsigned index; double output[3], brown_state; } source[] = {
    {0, {-0.7650185823440552, -0.1386140351826496, -0.0525012752589058}, -0.0150003643596874},
    {1, {0.6998904943466187, 0.0451803634227301, -0.0034401378967083}, -0.0009828965419167},
    {440, {0.3698917627334595, 0.0635560749356298, -0.0855325898434255}, -0.0244378828124073},
    {441, {-0.7482124567031860, -0.0988514521756854, -0.1352033939339691}, -0.0386295411239912},
    {6072, {0.9218870401382446, 0.5518269565058701, 0.6530074349451365}, 0.1865735528414676},
    {6412, {0.9171315431594849, 0.6557303341997140, 0.5005156677891132}, 0.1430044765111752},
    {1999999, {-0.1938178539276123, -0.1255906735012604, -0.1888645993177522}, -0.0539613140907864},
  };
  const uint32_t seeds[] = {1977, 0x9e3779b9u};
  for (unsigned seed = 0; seed < sizeof(seeds) / sizeof(seeds[0]); ++seed) {
    double roughness[3], rms[3];
    for (unsigned kind = 0; kind < 3; ++kind) {
      const device_config_sound_machine_sound_t sound = DEVICE_CONFIG_SOUND_MACHINE_WHITE_NOISE + kind;
      noise_state_t whole = {.seed = seeds[seed]}, chunked = {.seed = seeds[seed]};
      double sum = 0, squares = 0, differences = 0;
      float previous = 0, chunk[CHUNK_SAMPLES], output_peak = 0, state_peak = 0;
      unsigned beyond_old_limit = 0, checkpoint = 0;
      for (unsigned i = 0; i < 2000000; ++i) {
        const float value = noise_sample(&whole, sound);
        assert(isfinite(value));
        /* Render complete 441-sample chunks with one retained state; compare
         * every boundary against uninterrupted generation, for all 90 seconds. */
        if (i % CHUNK_SAMPLES == 0) {
          for (unsigned j = 0; j < CHUNK_SAMPLES; ++j)
            chunk[j] = noise_sample(&chunked, sound);
        }
        assert(value == chunk[i % CHUNK_SAMPLES]);
        for (unsigned j = 0; j < 7; ++j) assert(isfinite(whole.pink[j]));
        assert(isfinite(whole.brown));
        if (seed == 0 && checkpoint < sizeof(source) / sizeof(source[0]) &&
            i == source[checkpoint].index) {
          assert(fabs(value - source[checkpoint].output[kind]) < 1e-5);
          if (sound == DEVICE_CONFIG_SOUND_MACHINE_BROWN_NOISE)
            assert(fabs(whole.brown - source[checkpoint].brown_state) < 1e-5);
          ++checkpoint;
        }
        if (fabsf(value) > .65f) ++beyond_old_limit;
        output_peak = fmaxf(output_peak, fabsf(value));
        state_peak = fmaxf(state_peak, fabsf(whole.brown));
        sum += value; squares += value * value;
        differences += (value - previous) * (value - previous); previous = value;
      }
      if (seed == 0) assert(checkpoint == sizeof(source) / sizeof(source[0]));
      assert(fabs(sum / 2000000) < .02);
      rms[kind] = sqrt(squares / 2000000);
      roughness[kind] = differences / squares;
      /* The external equations give RMS .577/.194/.200 for these runs.
       * Broad bands catch scaling regressions without claiming loudness parity. */
      assert(rms[kind] > (kind == 0 ? .57 : .18));
      assert(rms[kind] < (kind == 0 ? .59 : .22));
      assert(beyond_old_limit > 0);
      if (sound == DEVICE_CONFIG_SOUND_MACHINE_WHITE_NOISE) assert(output_peak <= 1);
      if (sound == DEVICE_CONFIG_SOUND_MACHINE_BROWN_NOISE) {
        /* The source retains the unscaled integrator, not its 3.5x output. */
        assert(fabs(output_peak - state_peak * 3.5f) < 1e-6);
      }
      printf("noise seed=%u kind=%u peak=%.3f past_old_limit=%u brown_state_peak=%.3f\n",
             seeds[seed], kind, output_peak, beyond_old_limit, state_peak);
    }
    /* First-difference energy measures high-frequency content relative to total
     * energy. The source equations give about 2/.35/.039: White/Pink is 5.7x
     * and Pink/Brown is 8.9x. Require their clear separation with margin; the
     * former 20x Pink/Brown condition belonged to the altered Brown generator. */
    assert(roughness[0] > 3 * roughness[1]);
    assert(roughness[1] > 7 * roughness[2]);
    printf("actual noise generator on host: RMS %.3f/%.3f/%.3f, roughness %.4f/%.4f/%.4f\n",
           rms[0], rms[1], rms[2], roughness[0], roughness[1], roughness[2]);
  }
}

static const struct { const char *name; unsigned rate; uint64_t samples; unsigned frames; } assets[] = {
  {"rain",44100,1675512,1456}, {"thunder",44100,1896696,1648}, {"ocean",44100,1895544,1647},
  {"wind",44100,1675512,1456}, {"forest",24000,911616,1585}, {"fire",44100,1454328,1264},
  {"stream",44100,1454328,1264}, {"night",24000,911616,1585}, {"bowl",24000,1131648,1967},
};
static void metadata_checks(void)
{
  uint8_t data[MP3_INPUT_BYTES];
  for (unsigned asset = 0; asset < sizeof(assets) / sizeof(assets[0]); ++asset) {
    char path[1024]; snprintf(path, sizeof(path), "%s/%s.mp3", WAC_AUDIO_ASSET_DIR, assets[asset].name);
    FILE *file = fopen(path, "rb"); assert(file);
    const size_t count = fread(data, 1, sizeof(data), file);
    unsigned rate; uint64_t samples;
    assert(recording_metadata(data, count, &rate, &samples));
    assert(rate == assets[asset].rate && samples == assets[asset].samples);
    unsigned per_frame;
    const size_t first_size = encoded_frame_size(data, count, &per_frame);
    assert(!recording_metadata(data, first_size - 1, &rate, &samples));
    const uint8_t saved = data[3]; data[3] &= 0x3f; /* Unsupported stereo metadata. */
    assert(!recording_metadata(data, count, &rate, &samples)); data[3] = saved;
    rewind(file);
    unsigned frames = 0;
    for (;;) {
      const size_t read = fread(data, 1, 4, file);
      if (!read) { assert(feof(file) && !ferror(file)); break; }
      assert(read == 4);
      const size_t bytes = encoded_frame_size(data, read, &per_frame);
      assert(bytes >= 4 && bytes <= sizeof(data));
      assert(fread(data + 4, 1, bytes - 4, file) == bytes - 4);
      ++frames;
    }
    assert(frames == assets[asset].frames + 1);
    fclose(file);
    printf("actual asset %s: %uHz, %llu playable samples, %u physical frames\n",
           assets[asset].name, assets[asset].rate, (unsigned long long)assets[asset].samples, frames);
  }
  memset(data, 0, sizeof(data));
  unsigned rate; uint64_t samples;
  assert(!recording_metadata(data, sizeof(data), &rate, &samples));
}

static void machine_preferences(device_config_sound_machine_sound_t sound, unsigned volume, unsigned minutes)
{
  pthread_mutex_lock(&gate);
  applied.sound_machine_sound = sound; applied.sound_machine_volume = volume;
  applied.sound_machine_timer_minutes = minutes;
  pthread_mutex_unlock(&gate);
}
static void machine_stop(void)
{
  assert(wac_audio_sound_machine_stop() == ESP_OK);
  assert(!snapshot().machine_active && !snapshot().machine_pending);
  step(); assert(!at_write && !codec_open);
  assert(!snapshot().machine_active && !snapshot().machine_pending);
}
/* Independently derive amplitude from the approved dB values, rather than
 * copying the firmware's precomputed multipliers into the expected results. */
static double recording_boost(unsigned sound)
{
  const double db[] = {5.49, 1.12, 5.49, 5.49, 5.49, 15.49, 5.49, 15.49, 2.80};
  assert(sound < sizeof(db) / sizeof(db[0]));
  return pow(10, db[sound] / 20);
}
static void machine_gain_checks(void)
{
  const unsigned levels[] = {0, 37, 100};
  const int64_t remaining[] = {60000000, 30000000, 15000000, 1000000, 0, -1000000};
  for (unsigned sound = DEVICE_CONFIG_SOUND_MACHINE_RAIN;
       sound <= DEVICE_CONFIG_SOUND_MACHINE_BROWN_NOISE; ++sound) {
    machine_t machine = {.sound = sound, .timer_minutes = 15, .timer_started = true};
    const double boost = sound < DEVICE_CONFIG_SOUND_MACHINE_WHITE_NOISE ? recording_boost(sound) :
      pow(10, (sound == DEVICE_CONFIG_SOUND_MACHINE_WHITE_NOISE ? -12 : -6) / 20.0);
    for (unsigned v = 0; v < sizeof(levels) / sizeof(levels[0]); ++v) {
      const double full = levels[v] * levels[v] / 10000.0 * .66 * boost;
      for (unsigned r = 0; r < sizeof(remaining) / sizeof(remaining[0]); ++r) {
        machine.deadline = now + remaining[r];
        const double fade = fmax(0, fmin(1, remaining[r] / 30000000.0));
        assert(fabs(machine_gain(levels[v], &machine) - full * fade) < 1e-6);
      }
      /* Continuous playback and pending setup must ignore a stale deadline. */
      machine.timer_minutes = 0;
      assert(fabs(machine_gain(levels[v], &machine) - full) < 1e-6);
      machine.timer_minutes = 15; machine.timer_started = false;
      assert(fabs(machine_gain(levels[v], &machine) - full) < 1e-6);
      machine.timer_started = true;
    }
  }
}
static void machine_checks(void)
{
  machine_gain_checks();
  setup_delay = 2000000;
  assert(wac_audio_sound_machine_play() == ESP_OK && snapshot().machine_pending && !snapshot().machine_active);
  /* Navigation's preview Stop must neither erase pending Play nor stop audio. */
  assert(wac_audio_stop_preview() == ESP_OK && snapshot().machine_pending);
  step(); assert(at_write && codec_rate == SAMPLE_RATE && snapshot().machine_pending && !snapshot().machine_active);
  step(); assert(snapshot().machine_active && !snapshot().machine_pending);
  assert(snapshot().machine_remaining_seconds == 1800);
  assert(wac_audio_stop_preview() == ESP_OK); step(); assert(at_write && snapshot().machine_active);
  machine_preferences(DEVICE_CONFIG_SOUND_MACHINE_WHITE_NOISE, 0, 30);
  step(); assert(peak(last_pcm) == 0 && snapshot().machine_active);
  now += 40000000;
  const unsigned before_opens = opens;
  machine_preferences(DEVICE_CONFIG_SOUND_MACHINE_PINK_NOISE, 100, 30);
  step(); assert(opens == before_opens + 1 && snapshot().machine_remaining_seconds <= 1760);
  step(); assert(snapshot().machine_active && snapshot().machine_remaining_seconds <= 1760);
  const unsigned minutes[] = {15,30,60,120,0};
  for (unsigned i = 0; i < 5; ++i) {
    machine_preferences(DEVICE_CONFIG_SOUND_MACHINE_PINK_NOISE, 100, minutes[i]);
    step(); step();
    assert(snapshot().machine_continuous == (minutes[i] == 0));
    assert(snapshot().machine_remaining_seconds == minutes[i] * 60);
    now += 100000000;
    step(); assert(snapshot().machine_active);
    if (minutes[i]) assert(snapshot().machine_remaining_seconds <= minutes[i] * 60 - 100);
    else assert(snapshot().machine_remaining_seconds == 0);
  }
  /* The actual gain rule follows monotonic elapsed time; no RTC is read. */
  machine_t fade = {.sound = DEVICE_CONFIG_SOUND_MACHINE_WHITE_NOISE,
                    .timer_minutes = 15, .timer_started = true, .deadline = now + 30000000};
  const double white_trim = pow(10, -12 / 20.0);
  assert(fabs(machine_gain(100, &fade) - .66 * white_trim) < 1e-6);
  fade.deadline = now + 15000000;
  assert(fabs(machine_gain(100, &fade) - .33 * white_trim) < 1e-6);
  assert(fabs(machine_gain(50, &fade) - .0825 * white_trim) < 1e-6);
  fade.deadline = now; assert(machine_gain(100, &fade) == 0);
  machine_preferences(DEVICE_CONFIG_SOUND_MACHINE_BROWN_NOISE, 100, 15);
  step(); step(); assert(snapshot().machine_remaining_seconds == 900);
  now += 900000000; step(); assert(!at_write && !codec_open && !snapshot().machine_active);
  step(); assert(!at_write); /* Expiration never resumes automatically. */
  assert(wac_audio_sound_machine_play() == ESP_OK); step(); step();
  assert(snapshot().machine_active && snapshot().machine_remaining_seconds == 900);
  now += 100000000; step(); step();
  assert(snapshot().machine_remaining_seconds <= 800);
  assert(wac_audio_sound_machine_play() == ESP_OK); step(); step();
  assert(snapshot().machine_remaining_seconds == 900);
  assert(wac_audio_preview(DEVICE_CONFIG_ALARM_CLASSIC_BEEP, 50) == ESP_OK);
  assert(!snapshot().machine_active && wac_audio_sound_machine_play() == ESP_ERR_INVALID_STATE);
  step(); assert(at_write && snapshot().preview_active && !snapshot().machine_active);
  assert(wac_audio_sound_machine_play() == ESP_ERR_INVALID_STATE);
  assert(wac_audio_stop_preview() == ESP_OK);
  assert(!snapshot().preview_active && !snapshot().machine_active && !snapshot().machine_pending);
  step(); step(); assert(!at_write);
  assert(wac_audio_sound_machine_play() == ESP_OK); step(); step(); assert(snapshot().machine_active);
  alarm_phase(WAC_ALARM_RINGING, 42);
  assert(!snapshot().machine_active && wac_audio_sound_machine_play() == ESP_ERR_INVALID_STATE);
  step(); assert(at_write && !snapshot().machine_active && codec_rate == SAMPLE_RATE);
  alarm_phase(WAC_ALARM_SNOOZED, 42); step(); step(); assert(!at_write);
  alarm_phase(WAC_ALARM_IDLE, 42);
  assert(wac_audio_sound_machine_play() == ESP_OK); step(); step();
  machine_stop(); step(); assert(!at_write && creates == 1);
}

static uint64_t recording_chunk_check(unsigned asset, uint64_t position, double gain)
{
  for (unsigned i = 0; i < last_count; ++i) {
    const int expected = (int)lrint((1 + ((position + i) % assets[asset].samples) % 1000) * gain);
    /* One PCM count permits float gain rounding, while retaining the exact
     * synthetic decoder sequence across every tail/head boundary. */
    assert(abs(last_pcm[i] - expected) <= 1);
  }
  return (position + last_count) % assets[asset].samples;
}
static void recording_guard_checks(void)
{
  /* This is a constructed decoder case, not measured Campfire samples.
   * The approved larger gain makes the existing final conversion guard
   * consequential: loud samples must saturate, never wrap to the other sign. */
  guard_pcm_fixture = true;
  machine_preferences(DEVICE_CONFIG_SOUND_MACHINE_CAMPFIRE, 100, 0);
  assert(wac_audio_sound_machine_play() == ESP_OK); step();
  assert(at_write && codec_rate == 44100);
  const double gain = .66 * pow(10, 15.49 / 20.0);
  assert(12000 * gain > INT16_MAX && 3000 * gain < INT16_MAX);
  for (unsigned i = 0; i < last_count; ++i) {
    if (i % 4 == 0) assert(last_pcm[i] == INT16_MAX);
    else if (i % 4 == 1) assert(last_pcm[i] == INT16_MIN);
    else {
      const int expected = (int)lrint((i % 4 == 2 ? 3000 : -3000) * gain);
      assert(abs(last_pcm[i] - expected) <= 1);
    }
  }
  machine_stop();
  guard_pcm_fixture = false;
  assert(!recording_file && allocations_live == 0 && decoder_opens == decoder_closes);
}
static void recording_checks(void)
{
  machine_scenario = true;
  for (unsigned asset = 0; asset < sizeof(assets) / sizeof(assets[0]); ++asset) {
    machine_preferences(asset, 100, 0);
    const unsigned opened_before = opens, reset_before = decoder_resets, closed_before = decoder_closes;
    const unsigned files_before = file_opens, files_closed_before = file_closes;
    assert(wac_audio_sound_machine_play() == ESP_OK); step();
    assert(at_write && codec_rate == assets[asset].rate && !snapshot().machine_active);
    uint64_t position = 0;
    while (decoder_resets < reset_before + 2) {
      position = recording_chunk_check(asset, position, .66 * recording_boost(asset));
      step();
    }
    /* Check the mixed tail/head write itself, plus the next complete head. */
    for (unsigned chunk = 0; chunk < 2; ++chunk) {
      position = recording_chunk_check(asset, position, .66 * recording_boost(asset)); step();
    }
    assert(opens == opened_before + 1 && snapshot().machine_active && snapshot().machine_continuous);
    assert(file_opens == files_before + 3 && file_closes == files_closed_before + 2);
    /* The worker must apply the same boost after live volume changes and
     * during the existing final 30-second fade, without restarting decoding. */
    position = recording_chunk_check(asset, position, .66 * recording_boost(asset));
    const unsigned levels[] = {0, 37, 100};
    for (unsigned v = 0; v < sizeof(levels) / sizeof(levels[0]); ++v) {
      machine_preferences(asset, levels[v], 0); step();
      position = recording_chunk_check(asset, position,
        levels[v] * levels[v] / 10000.0 * .66 * recording_boost(asset));
    }
    machine_preferences(asset, 100, 15);
    const int64_t deadline = now + 900000000;
    step(); position = recording_chunk_check(asset, position, .66 * recording_boost(asset));
    now = deadline - 15000000;
    step(); position = recording_chunk_check(asset, position, .33 * recording_boost(asset));
    now = deadline - 1000000;
    step(); (void)recording_chunk_check(asset, position, .66 / 30 * recording_boost(asset));
    machine_stop(); assert(decoder_closes == closed_before + 1);
    assert(file_closes == files_closed_before + 3 && !recording_file);
  }
  assert(loops_verified == 18 && creates == 1 && mounts == 1 && allocations_live == 0);
  recording_guard_checks();
  puts("two repeats and gain/fade checks of all nine real assets, plus constructed PCM saturation, passed; no gapless board claim");
}

static void machine_failure_checks(void)
{
  machine_scenario = true;
  for (unsigned failure = 0; failure < 23; ++failure) {
    machine_preferences(DEVICE_CONFIG_SOUND_MACHINE_RAIN, 100, 0);
    mount_error = failure == 0 ? ESP_FAIL : ESP_OK;
    /* Retry mount must occur only after an explicit Play. */
    if (failure == 0) s_spiffs_mounted = false;
    missing_file = failure == 1; bad_metadata = failure == 2;
    decode_fail = failure == 3; decode_stall = failure == 4; format_fail = failure == 5;
    oversized = failure == 6; decode_short = failure == 7;
    open_error = failure == 8 ? ESP_FAIL : ESP_OK;
    volume_error = failure == 9 ? ESP_FAIL : ESP_OK;
    write_error = failure == 10 ? ESP_FAIL : ESP_OK;
    reset_fail = failure == 11;
    read_error = failure == 12;
    recovery_fail = failure == 13;
    allocation_fail_at = failure >= 14 && failure <= 16 ? allocation_attempts + failure - 13 : 0;
    fail_read_at = failure == 17 ? read_calls + 2 : 0;
    register_error = failure == 18 ? ESP_AUDIO_ERR_MEM_LACK : ESP_AUDIO_ERR_OK;
    if (failure == 18) s_mp3_registered = false;
    decoder_open_error = failure == 19 ? ESP_AUDIO_ERR_MEM_LACK : ESP_AUDIO_ERR_OK;
    info_error = failure == 20 ? ESP_AUDIO_ERR_FAIL : ESP_AUDIO_ERR_OK;
    fail_file_open_at = failure == 21 ? file_opens + 2 : 0;
    fail_file_close_at = failure == 22 ? file_closes + 1 : 0;
    const unsigned closed_before = decoder_closes, opened_before = decoder_opens;
    assert(wac_audio_sound_machine_play() == ESP_OK); step();
    while (at_write) step();
    assert(!codec_open && !snapshot().machine_active && !snapshot().machine_pending && snapshot().error != ESP_OK);
    assert(decoder_closes - closed_before == decoder_opens - opened_before && allocations_live == 0 && !recording_file);
    const unsigned before = decoder_opens;
    const unsigned files_before = file_opens;
    for (unsigned i = 0; i < 3; ++i) step();
    assert(decoder_opens == before && file_opens == files_before); /* No automatic retry. */
    mount_error = ESP_OK; missing_file = bad_metadata = decode_fail = decode_stall = format_fail = false;
    oversized = decode_short = reset_fail = read_error = recovery_fail = false;
    allocation_fail_at = fail_read_at = fail_file_open_at = fail_file_close_at = 0;
    register_error = decoder_open_error = info_error = ESP_AUDIO_ERR_OK;
    open_error = volume_error = write_error = ESP_OK;
    machine_preferences(failure >= 21 ? DEVICE_CONFIG_SOUND_MACHINE_RAIN : DEVICE_CONFIG_SOUND_MACHINE_WHITE_NOISE,
                        failure >= 21 ? 100 : 0, 0);
    assert(wac_audio_sound_machine_play() == ESP_OK); step(); step();
    assert(snapshot().machine_active && snapshot().error == ESP_OK);
    assert(failure >= 21 ? peak(last_pcm) > 0 : peak(last_pcm) == 0);
    machine_stop();
  }
  /* A bounded output-capacity retry retains the same encoded input. */
  machine_preferences(DEVICE_CONFIG_SOUND_MACHINE_FOREST, 100, 0);
  buffer_retry = true;
  assert(wac_audio_sound_machine_play() == ESP_OK); step(); step();
  assert(snapshot().machine_active && snapshot().error == ESP_OK);
  machine_stop(); assert(decoder_opens == decoder_closes);
  /* A failed live sound change ends the existing session; it never silently
   * falls back to the previous noise or retries the unavailable file. */
  machine_preferences(DEVICE_CONFIG_SOUND_MACHINE_WHITE_NOISE, 100, 30);
  assert(wac_audio_sound_machine_play() == ESP_OK); step(); step();
  assert(snapshot().machine_active);
  missing_file = true;
  machine_preferences(DEVICE_CONFIG_SOUND_MACHINE_RAIN, 100, 30);
  step(); assert(!at_write && !snapshot().machine_active && snapshot().error != ESP_OK);
  step(); assert(!at_write && allocations_live == 0);
  missing_file = false;
  assert(wac_audio_sound_machine_play() == ESP_OK); step(); step();
  assert(snapshot().machine_active && snapshot().machine_remaining_seconds == 1800);
  machine_stop(); assert(decoder_opens == decoder_closes && allocations_live == 0);
}

static void cancellation_checks(void)
{
  machine_scenario = true;
  for (unsigned stage = 0; stage < 5; ++stage) {
    machine_preferences(DEVICE_CONFIG_SOUND_MACHINE_RAIN, 100, 30);
    pause_mount = stage == 0; pause_decoder_open = stage == 1;
    pause_open = stage == 2; pause_decode = stage == 3; pause_read = stage == 4;
    if (stage == 0) s_spiffs_mounted = false;
    const unsigned before = writes;
    assert(wac_audio_sound_machine_play() == ESP_OK); step();
    assert(!at_write && snapshot().machine_pending && !snapshot().machine_active && writes == before);
    assert(wac_audio_sound_machine_stop() == ESP_OK); step();
    assert(!codec_open && writes == before && decoder_opens == decoder_closes && allocations_live == 0);
    assert(snapshot().error == ESP_OK);
    step(); assert(!at_write);
  }
  /* Stop during the repeat reopen prevents a mixed tail/head chunk from being
   * written and closes the newly opened stream exactly once. */
  machine_preferences(DEVICE_CONFIG_SOUND_MACHINE_RAIN, 100, 30);
  assert(wac_audio_sound_machine_play() == ESP_OK); step(); step();
  const unsigned resets_before = decoder_resets, files_before = file_opens;
  pause_file_open = true;
  while (at_write) step();
  assert(file_opens == files_before + 1 && recording_file && codec_open);
  const unsigned repeat_writes = writes;
  assert(wac_audio_sound_machine_stop() == ESP_OK); step();
  assert(!codec_open && !recording_file && writes == repeat_writes && decoder_resets == resets_before);
  assert(snapshot().error == ESP_OK && decoder_opens == decoder_closes && allocations_live == 0);
  step(); assert(!at_write);
  /* Preview navigation during a slow SM setup stays within its own scope. */
  pause_open = true;
  assert(wac_audio_sound_machine_play() == ESP_OK); step(); assert(!at_write);
  assert(wac_audio_stop_preview() == ESP_OK); step(); assert(at_write);
  step(); assert(snapshot().machine_active); machine_stop();
  /* Alarm arrival during setup must prevent the first recording write. */
  pause_decoder_open = true;
  assert(wac_audio_sound_machine_play() == ESP_OK); step();
  const unsigned before = writes;
  alarm_phase(WAC_ALARM_RINGING, 88); step();
  assert(at_write && codec_rate == SAMPLE_RATE && writes == before + 1 && !snapshot().machine_active);
  alarm_phase(WAC_ALARM_IDLE, 88); step(); step();
  assert(!at_write && !snapshot().machine_pending && decoder_opens == decoder_closes);
}

static void weather_generator_checks(void)
{
  const unsigned starts[][6] = {{0, 2, 4}, {0, 5, 8, 11, 16, 19}};
  const unsigned lengths[][6] = {{1, 1, 1}, {4, 2, 2, 4, 2, 2}};
  unsigned alarm_peak = 0;
  for (unsigned sound = 0; sound < 3; ++sound) {
    bell_state_t bell = {0};
    int16_t pcm[CHUNK_SAMPLES];
    for (unsigned chunk = 0; chunk < 250; ++chunk) {
      fill_chunk(pcm, chunk * CHUNK_SAMPLES, sound, 100, true, 30, &bell);
      if (peak(pcm) > alarm_peak) alarm_peak = peak(pcm);
    }
  }
  printf("Independent complete alarm-pattern maximum: %u PCM\n", alarm_peak);
  for (unsigned kind = 0; kind < 2; ++kind) {
    const unsigned expected_frames = kind ? 231525 : 55125;
    assert(weather_frames(kind) == expected_frames);
    double band[4] = {0}, quadrature[4] = {0};
    const double frequencies[] = {1050, 853, 960, 700};
    float maximum = 0;
    for (unsigned i = 0; i < expected_frames + CHUNK_SAMPLES; ++i) {
      const float value = weather_sample(kind, i);
      bool audible = false;
      for (unsigned burst = 0; burst < (kind ? 6u : 3u); ++burst) {
        unsigned start = starts[kind][burst] * 11025;
        unsigned end = start + lengths[kind][burst] * 11025;
        if (i >= start && i < end) audible = true;
        if (i == start + 200) assert(fabsf(value) > .01f);
      }
      if (!audible) assert(value == 0);
      maximum = fmaxf(maximum, fabsf(value));
      if (i < 8000) for (unsigned f = 0; f < 4; ++f) {
        double phase = 2 * 3.14159265358979323846 * frequencies[f] * i / 22050;
        band[f] += value * cos(phase);
        quadrature[f] += value * sin(phase);
      }
    }
    assert(maximum > .80f && maximum <= .80501f);
    assert(fabs(maximum * .82 * INT16_MAX - alarm_peak) < 100);
    for (unsigned f = 0; f < 4; ++f) band[f] = hypot(band[f], quadrature[f]);
    if (!kind) assert(band[0] > 20 * band[1] && band[0] > 20 * band[2]);
    else assert(band[1] > 20 * band[0] && band[2] > 20 * band[0]);
    assert((kind ? band[1] : band[0]) > 20 * band[3]);
  }
}

static uint32_t weather_start(wac_audio_weather_kind_t kind)
{
  uint32_t token = 0;
  assert(wac_audio_weather_start(kind, &token) == ESP_OK && token);
  assert(snapshot().weather_token == token && snapshot().weather_state == WAC_AUDIO_WEATHER_PENDING);
  return token;
}

static void weather_stop_ack(uint32_t token)
{
  assert(wac_audio_weather_stop(token) == ESP_OK);
  step();
  assert(snapshot().weather_token == token && snapshot().weather_state == WAC_AUDIO_WEATHER_CANCELED);
  wac_audio_weather_ack(token);
  assert(snapshot().weather_state == WAC_AUDIO_WEATHER_NONE);
}

static void weather_checks(void)
{
  machine_scenario = true;
  uint32_t rejected = 999;
  assert(wac_audio_weather_start(99, &rejected) == ESP_ERR_INVALID_ARG);
  assert(wac_audio_weather_start(WAC_AUDIO_WEATHER_WATCH, NULL) == ESP_ERR_INVALID_ARG);
  uint32_t token = weather_start(WAC_AUDIO_WEATHER_WATCH);
  assert(wac_audio_weather_start(WAC_AUDIO_WEATHER_WARNING, &rejected) == ESP_ERR_INVALID_STATE && !rejected);
  assert(wac_audio_weather_stop(token) == ESP_OK);
  step(); assert(!at_write && snapshot().weather_state == WAC_AUDIO_WEATHER_CANCELED);
  assert(!snapshot().weather_started_us);
  wac_audio_weather_ack(token);

  pause_creation = true;
  token = weather_start(WAC_AUDIO_WEATHER_WATCH);
  const unsigned creation_writes = writes;
  step(); assert(!at_write && !codec_open && creates == 1);
  weather_stop_ack(token); assert(writes == creation_writes);

  for (unsigned kind = 0; kind < 2; ++kind) {
    token = weather_start(kind);
    setup_delay = 3000000;
    const unsigned before = writes;
    step(); assert(at_write && codec_rate == SAMPLE_RATE);
    const int64_t first = snapshot().weather_started_us;
    assert(first && first == now - 20000 && snapshot().weather_state == WAC_AUDIO_WEATHER_PLAYING);
    assert(wac_audio_preview(DEVICE_CONFIG_ALARM_CLASSIC_BEEP, 50) == ESP_ERR_INVALID_STATE);
    assert(wac_audio_stop_preview() == ESP_OK);
    while (at_write) step();
    assert(writes - before == (kind ? 525u : 125u));
    assert(!codec_open && snapshot().weather_state == WAC_AUDIO_WEATHER_FINISHED);
    assert(snapshot().weather_started_us == first);
    const unsigned finished = writes;
    for (unsigned i = 0; i < 3; ++i) step();
    assert(writes == finished);
    assert(wac_audio_weather_start(kind, &rejected) == ESP_ERR_INVALID_STATE);
    wac_audio_weather_ack(token + 1); assert(snapshot().weather_token == token);
    wac_audio_weather_ack(token); assert(snapshot().weather_state == WAC_AUDIO_WEATHER_NONE);
  }
  /* Applied alarm volume, including zero, never uses a preview draft. */
  for (unsigned volume = 0; volume < 3; ++volume) {
    const unsigned levels[] = {0, 37, 100};
    applied.alarm_volume = levels[volume];
    token = weather_start(WAC_AUDIO_WEATHER_WATCH);
    step(); assert(at_write);
    for (unsigned i = 0; i < CHUNK_SAMPLES; ++i) {
      const double sample = weather_sample(WAC_AUDIO_WEATHER_WATCH, i) * .82 *
        levels[volume] * levels[volume] / 10000 * INT16_MAX;
      assert(abs(last_pcm[i] - (int)lrint(sample)) <= 1);
    }
    weather_stop_ack(token); assert(!codec_open);
  }
  applied.alarm_volume = 50;
  /* Weather stops an existing preview and leaves it stopped. */
  assert(wac_audio_preview(DEVICE_CONFIG_ALARM_CLASSIC_BELL, 37) == ESP_OK);
  step(); assert(snapshot().preview_active);
  token = weather_start(WAC_AUDIO_WEATHER_WATCH);
  assert(!snapshot().preview_active);
  step(); assert(at_write && !snapshot().preview_active);
  weather_stop_ack(token); step(); assert(!at_write && !snapshot().preview_active);

  const device_config_sound_machine_sound_t sounds[] = {
    DEVICE_CONFIG_SOUND_MACHINE_WHITE_NOISE, DEVICE_CONFIG_SOUND_MACHINE_RAIN,
    DEVICE_CONFIG_SOUND_MACHINE_FOREST};
  for (unsigned sound = 0; sound < 3; ++sound) {
    machine_preferences(sounds[sound], 37, 15);
    assert(wac_audio_sound_machine_play() == ESP_OK); step(); step();
    assert(snapshot().machine_active);
    const unsigned rate = codec_rate, decoders = decoder_opens;
    const int64_t original_end = now - 20000 + INT64_C(900000000);
    token = weather_start(WAC_AUDIO_WEATHER_WARNING);
    step(); assert(at_write && codec_rate == SAMPLE_RATE && snapshot().machine_active);
    assert(wac_audio_sound_machine_play() == ESP_ERR_INVALID_STATE);
    const int64_t first = snapshot().weather_started_us;
    while (snapshot().weather_state == WAC_AUDIO_WEATHER_PLAYING) step();
    assert(at_write && codec_rate == rate && decoder_opens == decoders);
    step();
    assert(snapshot().machine_active && snapshot().machine_remaining_seconds <= 890);
    assert(snapshot().weather_started_us == first && now < original_end);
    wac_audio_weather_ack(token);
    assert(snapshot().machine_remaining_seconds < 900);
    if (sounds[sound] < DEVICE_CONFIG_SOUND_MACHINE_WHITE_NOISE) {
      /* The simulated recording's output was advanced by the two pre-alert
       * chunks; resume retains decoder position, independent volume and fade. */
      uint64_t position = (uint64_t)rate / 50 * 3;
      unsigned asset = sounds[sound];
      (void)recording_chunk_check(asset, position, .37 * .37 * .66 * recording_boost(asset));
    }
    machine_stop();
  }

  for (unsigned action = 0; action < 4; ++action) {
    machine_preferences(DEVICE_CONFIG_SOUND_MACHINE_WHITE_NOISE, 37, 15);
    assert(wac_audio_sound_machine_play() == ESP_OK); step(); step();
    token = weather_start(WAC_AUDIO_WEATHER_WARNING); step();
    assert(snapshot().machine_active);
    if (action == 0) assert(wac_audio_sound_machine_stop() == ESP_OK);
    if (action == 1) now += INT64_C(900000000);
    if (action == 2) machine_preferences(DEVICE_CONFIG_SOUND_MACHINE_BROWN_NOISE, 7, 30);
    if (action == 3) alarm_phase(WAC_ALARM_RINGING, 200);
    if (action != 3) {
      assert(wac_audio_weather_stop(token) == ESP_OK);
      step();
      if (action < 2) assert(!at_write && !snapshot().machine_active);
      else {
        assert(at_write && (snapshot().machine_active || snapshot().machine_pending) && snapshot().machine_remaining_seconds == 1800);
        step(); machine_stop();
      }
    } else {
      step(); assert(at_write && !snapshot().machine_active);
      assert(snapshot().weather_state == WAC_AUDIO_WEATHER_SKIPPED_RING);
      alarm_phase(WAC_ALARM_SNOOZED, 200); step(); step();
      assert(!at_write && !snapshot().machine_active);
      alarm_phase(WAC_ALARM_IDLE, 200);
    }
    wac_audio_weather_ack(token);
    step(); assert(!at_write);
  }
  /* A fresh Play submitted after weather Stop replaces the retained session,
   * including Stop/Play overwritten before the worker observes either. */
  machine_preferences(DEVICE_CONFIG_SOUND_MACHINE_WHITE_NOISE, 37, 15);
  assert(wac_audio_sound_machine_play() == ESP_OK); step(); step();
  now += 100000000;
  token = weather_start(WAC_AUDIO_WEATHER_WARNING); step();
  assert(wac_audio_sound_machine_stop() == ESP_OK);
  assert(wac_audio_weather_stop(token) == ESP_OK);
  assert(wac_audio_sound_machine_play() == ESP_OK);
  step(); step();
  assert(snapshot().machine_active && snapshot().machine_remaining_seconds == 900);
  wac_audio_weather_ack(token); machine_stop();
  /* Stop during the resume open cancels the retained machine before PCM. */
  assert(wac_audio_sound_machine_play() == ESP_OK); step(); step();
  token = weather_start(WAC_AUDIO_WEATHER_WARNING); step();
  pause_open = true;
  assert(wac_audio_weather_stop(token) == ESP_OK);
  step(); assert(!at_write && codec_open);
  const unsigned resume_writes = writes;
  assert(wac_audio_sound_machine_stop() == ESP_OK);
  step(); assert(!at_write && writes == resume_writes && !codec_open);
  wac_audio_weather_ack(token);
  /* Cancel/ack from an earlier request cannot affect its successor. */
  const uint32_t old = token;
  token = weather_start(WAC_AUDIO_WEATHER_WARNING);
  assert(wac_audio_weather_stop(old) == ESP_OK); wac_audio_weather_ack(old);
  step(); assert(at_write && snapshot().weather_token == token);
  weather_stop_ack(token);
  /* Ringing before the worker consumes a pending weather request skips it. */
  token = weather_start(WAC_AUDIO_WEATHER_WATCH);
  alarm_phase(WAC_ALARM_RINGING, 201); step();
  assert(at_write && snapshot().weather_state == WAC_AUDIO_WEATHER_SKIPPED_RING);
  assert(!snapshot().weather_started_us);
  assert(wac_audio_weather_start(WAC_AUDIO_WEATHER_WARNING, &rejected) == ESP_ERR_INVALID_STATE);
  wac_audio_weather_ack(token);
  alarm_phase(WAC_ALARM_IDLE, 201); step(); step(); assert(!at_write);
  /* Slow speaker open/volume setup observes cancellation/ringing before PCM. */
  for (unsigned stage = 0; stage < 4; ++stage) {
    const unsigned ring = stage % 2;
    pause_open = stage < 2; pause_volume = stage >= 2;
    token = weather_start(WAC_AUDIO_WEATHER_WATCH);
    const unsigned before = writes;
    step(); assert(!at_write && codec_open && !snapshot().weather_started_us);
    if (ring) alarm_phase(WAC_ALARM_RINGING, 202);
    else assert(wac_audio_weather_stop(token) == ESP_OK);
    step();
    assert(writes == before + ring);
    assert(snapshot().weather_state == (ring ? WAC_AUDIO_WEATHER_SKIPPED_RING : WAC_AUDIO_WEATHER_CANCELED));
    assert(!snapshot().weather_started_us);
    wac_audio_weather_ack(token);
    if (ring) { alarm_phase(WAC_ALARM_IDLE, 202); step(); }
    step(); assert(!at_write);
  }
  /* Weather arrives during each slow machine setup/read boundary. Preserve a
   * pending session without starting its elapsed timer before its first PCM. */
  for (unsigned stage = 0; stage < 6; ++stage) {
    machine_preferences(DEVICE_CONFIG_SOUND_MACHINE_RAIN, 37, 15);
    pause_mount = stage == 0; pause_file_open = stage == 1;
    pause_decoder_open = stage == 2; pause_open = stage == 3;
    pause_read = stage == 4; pause_decode = stage == 5;
    if (stage == 0) s_spiffs_mounted = false;
    assert(wac_audio_sound_machine_play() == ESP_OK);
    const unsigned before = writes;
    step(); assert(!at_write && writes == before);
    token = weather_start(WAC_AUDIO_WEATHER_WATCH);
    step(); assert(at_write && codec_rate == SAMPLE_RATE && snapshot().machine_active);
    assert(snapshot().machine_remaining_seconds == 900);
    assert(wac_audio_weather_stop(token) == ESP_OK);
    step(); assert(at_write && codec_rate == 44100);
    step(); assert(snapshot().machine_active && snapshot().machine_remaining_seconds == 900);
    wac_audio_weather_ack(token); machine_stop();
  }
  /* Weather also preempts preview while its initial speaker open is blocked. */
  pause_open = true;
  assert(wac_audio_preview(DEVICE_CONFIG_ALARM_CLASSIC_BELL, 37) == ESP_OK);
  const unsigned preview_writes = writes;
  step(); assert(!at_write);
  token = weather_start(WAC_AUDIO_WEATHER_WATCH);
  step(); assert(at_write && writes == preview_writes + 1 && !snapshot().preview_active);
  weather_stop_ack(token); step(); assert(!at_write);
  assert(allocations_live == 0 && decoder_opens == decoder_closes && !recording_file);
}

static void weather_failure_checks(bool creation_failure)
{
  machine_scenario = true;
  for (unsigned failure = 0; failure < (creation_failure ? 1u : 4u); ++failure) {
    if (!creation_failure) {
      machine_preferences(DEVICE_CONFIG_SOUND_MACHINE_RAIN, 37, 15);
      assert(wac_audio_sound_machine_play() == ESP_OK); step(); step();
      assert(snapshot().machine_active);
    }
    const esp_err_t previous_preview_error = snapshot().error;
    uint32_t token = weather_start(WAC_AUDIO_WEATHER_WARNING);
    bsp_fail = creation_failure;
    open_error = failure == 0 && !creation_failure ? ESP_FAIL : ESP_OK;
    volume_error = failure == 1 ? ESP_FAIL : ESP_OK;
    write_error = ESP_OK;
    step();
    if (failure == 2) { write_error = ESP_FAIL; step(); }
    if (failure == 3) {
      assert(at_write);
      close_error = ESP_FAIL;
      assert(wac_audio_weather_stop(token) == ESP_OK); step();
    }
    assert(!at_write && !codec_open && !snapshot().machine_active && !snapshot().machine_pending);
    assert(snapshot().weather_state == WAC_AUDIO_WEATHER_FAILED);
    assert(snapshot().weather_error != ESP_OK);
    assert(snapshot().error == previous_preview_error);
    assert((snapshot().weather_started_us != 0) == (failure >= 2));
    const unsigned before = opens;
    for (unsigned i = 0; i < 3; ++i) step();
    assert(opens == before && allocations_live == 0 && decoder_opens == decoder_closes);
    wac_audio_weather_ack(token);
    open_error = volume_error = write_error = close_error = ESP_OK;
  }
  if (creation_failure) {
    assert(!snapshot().ready && wac_audio_init() == ESP_ERR_INVALID_STATE);
    return;
  }
  /* A failure in the actual machine resume remains a machine error, whereas
   * a weather attempt failure must not claim the unrelated preview failed. */
  assert(wac_audio_sound_machine_play() == ESP_OK); step(); step();
  const uint32_t token = weather_start(WAC_AUDIO_WEATHER_WATCH); step();
  open_error = ESP_FAIL;
  assert(wac_audio_weather_stop(token) == ESP_OK); step();
  assert(!at_write && !snapshot().machine_active && snapshot().error == ESP_FAIL);
  assert(snapshot().weather_state == WAC_AUDIO_WEATHER_CANCELED && snapshot().weather_error == ESP_OK);
  wac_audio_weather_ack(token);
  open_error = ESP_OK;
}

int main(int argc, char **argv)
{
  caller_thread = pthread_self();
  assert(argc == 2);
  const char *scenario = argv[1];
  if (!strcmp(scenario, "noise")) { noise_checks(); return 0; }
  if (!strcmp(scenario, "metadata")) { metadata_checks(); return 0; }
  if (!strcmp(scenario, "weathergenerator")) { weather_generator_checks(); return 0; }
  if (!strcmp(scenario, "generator")) { generator_checks(); puts("audio generator passed"); return 0; }
  assert(wac_audio_preview(DEVICE_CONFIG_ALARM_CLASSIC_BEEP, 50) == ESP_ERR_INVALID_STATE);
  assert(wac_audio_stop_preview() == ESP_ERR_INVALID_STATE);
  assert(wac_audio_sound_machine_play() == ESP_ERR_INVALID_STATE);
  assert(wac_audio_sound_machine_stop() == ESP_ERR_INVALID_STATE);
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
  if (!strcmp(scenario, "weather")) weather_checks();
  else if (!strcmp(scenario, "weatherfail")) weather_failure_checks(false);
  else if (!strcmp(scenario, "weatherbspfail")) weather_failure_checks(true);
  else if (!strcmp(scenario, "playback")) playback_checks();
  else if (!strcmp(scenario, "machine")) machine_checks();
  else if (!strcmp(scenario, "recording")) recording_checks();
  else if (!strcmp(scenario, "machinefail")) machine_failure_checks();
  else if (!strcmp(scenario, "cancelsetup")) cancellation_checks();
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
