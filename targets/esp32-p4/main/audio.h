#pragma once

#include "device_config.h"

typedef struct {
  /* Worker exists, and speaker creation has not permanently failed. Speaker
   * setup is lazy, so ready does not claim that hardware has been tested. */
  bool ready;
  /* Includes a queued Start; a queued Stop makes this false immediately. */
  bool preview_active;
  bool machine_active;
  bool machine_pending;
  bool machine_continuous;
  uint32_t machine_remaining_seconds;
  esp_err_t error;
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
