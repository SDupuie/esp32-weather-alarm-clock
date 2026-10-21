#pragma once

#include "device_config.h"

typedef enum {
  WAC_AUDIO_WEATHER_WATCH,
  WAC_AUDIO_WEATHER_WARNING,
} wac_audio_weather_kind_t;

typedef enum {
  WAC_AUDIO_WEATHER_NONE,
  WAC_AUDIO_WEATHER_PENDING,
  WAC_AUDIO_WEATHER_PLAYING,
  WAC_AUDIO_WEATHER_FINISHED,
  WAC_AUDIO_WEATHER_CANCELED,
  WAC_AUDIO_WEATHER_SKIPPED_RING,
  WAC_AUDIO_WEATHER_FAILED,
} wac_audio_weather_state_t;

typedef struct {
  /* Worker exists, and speaker creation has not permanently failed. Speaker
   * setup is lazy, so ready does not claim that hardware has been tested. */
  bool ready;
  /* Includes a queued Start; a queued Stop makes this false immediately. */
  bool preview_active;
  /* Includes a valid session temporarily interrupted by weather audio. */
  bool machine_active;
  bool machine_pending;
  bool machine_continuous;
  uint32_t machine_remaining_seconds;
  esp_err_t error;
  uint32_t weather_token;
  wac_audio_weather_kind_t weather_kind;
  wac_audio_weather_state_t weather_state;
  /* First attempted PCM submission, retained even on partial-write failure. */
  int64_t weather_started_us;
  esp_err_t weather_error;
} wac_audio_snapshot_t;

/* Startup only, after BSP display initialization. Creates one worker; no
 * speaker operation occurs in the caller or under the display lock. */
esp_err_t wac_audio_init(void);
/* Copies draft values. Latest Start/Stop replaces the previous pending command.
 * Already ringing rejects Start; newly ringing discards pending previews.
 * These calls never wait for hardware or playback. */
esp_err_t wac_audio_preview(device_config_alarm_sound_t sound, uint8_t volume);
esp_err_t wac_audio_stop_preview(void);
/* Applied preferences come from device_config. Calls only submit a bounded
 * action; Stop cancels an in-progress setup as well as active playback. */
esp_err_t wac_audio_sound_machine_play(void);
esp_err_t wac_audio_sound_machine_stop(void);
void wac_audio_copy(wac_audio_snapshot_t *out);

/* One outstanding weather request; busy/ringing requests are skipped, never
 * queued. The terminal receipt persists until its matching acknowledgement.
 * Tokens scope cancellation; stale Stop/ack calls cannot affect a newer request.
 * Eligibility and same-event sound separation belong to the weather owner. */
esp_err_t wac_audio_weather_start(wac_audio_weather_kind_t kind, uint32_t *token);
esp_err_t wac_audio_weather_stop(uint32_t token);
void wac_audio_weather_ack(uint32_t token);
