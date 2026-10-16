#include "alarm.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "nvs.h"

#define ALARM_NAMESPACE "wac_alarm"
#define ALARM_RECORD_KEY "operating"
#define ONE_TIME_KEY "one_time"
#define CATCHUP_SECONDS (30 * 60)

/* Explicit ordinary operating data, with no compiler-dependent bool/time_t
 * fields. A zero date means absent. This is one current record, not history. */
typedef struct {
  int64_t utc;
  int32_t date;
  uint16_t minute;
  uint16_t reserved;
} stored_occurrence_t;
typedef struct {
  stored_occurrence_t handled;
  stored_occurrence_t snooze;
  stored_occurrence_t missed;
  int64_t snooze_deadline;
} alarm_record_t;
_Static_assert(sizeof(stored_occurrence_t) == 16 && sizeof(alarm_record_t) == 56,
               "alarm operating record must retain its explicit fixed size");

static portMUX_TYPE s_copy_lock = portMUX_INITIALIZER_UNLOCKED;
static wac_alarm_snapshot_t s_copy;
/* Private to the single clock-control writer. Ringing is deliberately RAM-only;
 * handled storage covers dismissal, snooze, missed and idle-skipped occurrences. */
static wac_alarm_snapshot_t s_state;
static alarm_record_t s_record;
static wac_alarm_occurrence_t s_handled;
static bool s_initialized;
/* Startup recovery differs from runtime enable or schedule editing. Arming
 * without usable time waits before choosing its first future occurrence. */
static bool s_config_observed;
static bool s_enabled;
static uint8_t s_hour, s_minute, s_repeat_days;
static bool s_arm_next_pending;
/* The extra dated target is needed only by Repeat None. Its reserved field is
 * 0 while armed and 1 once consumed, including when saving Alarm Off fails. */
static stored_occurrence_t s_one_time;
static bool s_target_dirty, s_complete_pending;

static stored_occurrence_t store_occurrence(wac_alarm_occurrence_t value)
{
  if (!value.valid) return (stored_occurrence_t){0};
  return (stored_occurrence_t){ .utc = value.utc, .date = value.local_date,
    .minute = value.minute };
}
static wac_alarm_occurrence_t load_occurrence(stored_occurrence_t value)
{
  return (wac_alarm_occurrence_t){ .valid = value.date != 0,
    .utc = (time_t)value.utc, .local_date = value.date, .minute = value.minute };
}
static bool stored_valid(stored_occurrence_t value)
{
  if (!value.date) return value.utc == 0 && value.minute == 0 && value.reserved == 0;
  int month = value.date / 100 % 100, day = value.date % 100;
  return value.date >= 19700101 && value.date <= 99991231
    && month >= 1 && month <= 12 && day >= 1 && day <= 31
    && value.minute < 24 * 60 && value.reserved == 0
    && (int64_t)(time_t)value.utc == value.utc;
}
static void publish(void)
{
  portENTER_CRITICAL(&s_copy_lock);
  s_copy = s_state;
  portEXIT_CRITICAL(&s_copy_lock);
}
void wac_alarm_copy(wac_alarm_snapshot_t *out)
{
  if (!out) return;
  portENTER_CRITICAL(&s_copy_lock);
  *out = s_copy;
  portEXIT_CRITICAL(&s_copy_lock);
}
static esp_err_t save_changes(bool operating)
{
  /* Callers publish the live action first. No copy lock spans storage I/O. */
  esp_err_t err = ESP_OK;
  /* An explicit Snooze/Dismiss action also saves its current target, so a
   * later successful action can retain a date whose first save failed. */
  bool target = s_target_dirty || (operating && s_one_time.date);
  bool complete = s_complete_pending;
  s_target_dirty = s_complete_pending = false; /* No automatic tick retries. */
  if (operating || target) {
    nvs_handle_t handle;
    err = nvs_open(ALARM_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
      if (target) err = nvs_set_blob(handle, ONE_TIME_KEY, &s_one_time, sizeof(s_one_time));
      if (err == ESP_OK && operating)
        err = nvs_set_blob(handle, ALARM_RECORD_KEY, &s_record, sizeof(s_record));
      if (err == ESP_OK) err = nvs_commit(handle);
      nvs_close(handle);
    }
  }
  if (complete) {
    esp_err_t off = device_config_complete_one_time(s_one_time.minute / 60,
                                                   s_one_time.minute % 60);
    if (err == ESP_OK) err = off;
  }
  s_state.storage_error = err;
  publish();
  return err;
}
static bool same_occurrence(wac_alarm_occurrence_t a, wac_alarm_occurrence_t b)
{
  return a.valid && b.valid && a.utc == b.utc
    && a.local_date == b.local_date && a.minute == b.minute;
}
static void consume_one_time(wac_alarm_occurrence_t occurrence)
{
  if (same_occurrence(occurrence, load_occurrence(s_one_time)) && !s_one_time.reserved) {
    s_one_time.reserved = 1;
    s_target_dirty = s_complete_pending = true;
    s_state.next = (wac_alarm_occurrence_t){0};
  }
}
static bool handled(wac_alarm_occurrence_t occurrence)
{
  return s_handled.valid && (occurrence.utc <= s_handled.utc
    || (occurrence.local_date == s_handled.local_date
        && occurrence.minute == s_handled.minute));
}
static void remember(wac_alarm_occurrence_t occurrence, bool persist)
{
  if (!s_handled.valid || occurrence.utc > s_handled.utc) s_handled = occurrence;
  if (persist) {
    wac_alarm_occurrence_t saved = load_occurrence(s_record.handled);
    if (!saved.valid || occurrence.utc > saved.utc)
      s_record.handled = store_occurrence(occurrence);
  }
}
static bool clear_snooze(void)
{
  bool changed = s_record.snooze.date != 0 || s_record.snooze_deadline != 0;
  s_record.snooze = (stored_occurrence_t){0};
  s_record.snooze_deadline = 0;
  s_state.snooze_deadline = 0;
  return changed;
}
static bool start_ring(wac_alarm_occurrence_t occurrence)
{
  /* A resumed snooze is not finished until Dismiss. Retain its stored deadline
   * so a restart during the sound still follows the approved overdue rule. */
  bool changed = false;
  s_state.snooze_deadline = 0;
  if (s_state.missed.valid) {
    s_state.missed = (wac_alarm_occurrence_t){0};
    s_record.missed = (stored_occurrence_t){0};
    changed = true;
  }
  s_state.phase = WAC_ALARM_RINGING;
  s_state.current = occurrence;
  remember(occurrence, false);
  return changed;
}
esp_err_t wac_alarm_init(void)
{
  s_record = (alarm_record_t){0};
  s_state = (wac_alarm_snapshot_t){0};
  s_handled = (wac_alarm_occurrence_t){0};
  s_one_time = (stored_occurrence_t){0};
  s_target_dirty = s_complete_pending = false;
  s_config_observed = s_enabled = s_arm_next_pending = false;
  s_hour = s_minute = s_repeat_days = 0;
  nvs_handle_t handle;
  esp_err_t err = nvs_open(ALARM_NAMESPACE, NVS_READONLY, &handle);
  if (err == ESP_OK) {
    alarm_record_t loaded = {0};
    size_t size = sizeof(loaded);
    err = nvs_get_blob(handle, ALARM_RECORD_KEY, &loaded, &size);
    if (err == ESP_OK && (size != sizeof(loaded)
        || !stored_valid(loaded.handled) || !stored_valid(loaded.snooze)
        || !stored_valid(loaded.missed)
        || (int64_t)(time_t)loaded.snooze_deadline != loaded.snooze_deadline
        || (!!loaded.snooze.date != !!loaded.snooze_deadline)))
      err = ESP_ERR_INVALID_RESPONSE;
    if (err == ESP_OK) s_record = loaded;
    if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
    if (err == ESP_OK) {
      stored_occurrence_t target = {0};
      size = sizeof(target);
      err = nvs_get_blob(handle, ONE_TIME_KEY, &target, &size);
      uint16_t consumed = target.reserved;
      target.reserved = 0;
      if (err == ESP_OK && (size != sizeof(target) || !stored_valid(target)
          || consumed > 1 || (!target.date && consumed))) err = ESP_ERR_INVALID_RESPONSE;
      if (err == ESP_OK) {
        target.reserved = consumed;
        s_one_time = target;
        s_complete_pending = consumed != 0;
      }
    }
    nvs_close(handle);
  }
  if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
  s_handled = load_occurrence(s_record.handled);
  s_state.missed = load_occurrence(s_record.missed);
  if (s_record.snooze.date) {
    s_state.phase = WAC_ALARM_SNOOZED;
    s_state.current = load_occurrence(s_record.snooze);
    s_state.snooze_deadline = (time_t)s_record.snooze_deadline;
    remember(s_state.current, false);
  }
  s_state.storage_error = err;
  s_initialized = true;
  publish();
  return err;
}
esp_err_t wac_alarm_update(const device_config_t *config, const wac_time_snapshot_t *clock)
{
  if (!config || !clock) return ESP_ERR_INVALID_ARG;
  if (!s_initialized) return ESP_ERR_INVALID_STATE;
  if (s_config_observed && config->alarm_enabled
      && (!s_enabled || s_hour != config->alarm_hour
          || s_minute != config->alarm_minute
          || s_repeat_days != config->alarm_repeat_days))
    s_arm_next_pending = true;
  s_config_observed = true;
  s_enabled = config->alarm_enabled;
  s_hour = config->alarm_hour;
  s_minute = config->alarm_minute;
  s_repeat_days = config->alarm_repeat_days;
  if (!config->alarm_enabled) {
    s_arm_next_pending = false;
    s_complete_pending = false;
    s_state.next = (wac_alarm_occurrence_t){0};
    /* Off cancels captured ringing/snooze even before usable time arrives.
     * Reuse Dismiss so cancellation has the same save/error semantics. */
    if (s_state.phase != WAC_ALARM_IDLE) return wac_alarm_dismiss();
    publish();
    return ESP_OK;
  }
  if (!clock->usable) {
    s_state.next = (wac_alarm_occurrence_t){0};
    publish();
    return ESP_OK;
  }
  wac_alarm_schedule_t schedule;
  wac_alarm_schedule_calculate(config, clock->utc_now, &schedule);
  bool save = false;
  bool one_time = config->alarm_repeat_days == 0;
  if (one_time) {
    if (s_arm_next_pending || !s_one_time.date
        || s_one_time.minute != config->alarm_hour * 60 + config->alarm_minute) {
      s_one_time = store_occurrence(schedule.next);
      s_target_dirty = s_one_time.date != 0;
      s_complete_pending = false;
    }
    wac_alarm_occurrence_t target = load_occurrence(s_one_time);
    schedule = (wac_alarm_schedule_t){0};
    if (target.valid && !s_one_time.reserved) {
      if (target.utc <= clock->utc_now) schedule.previous = target;
      if (target.utc >= clock->utc_now) schedule.next = target;
    } else if (same_occurrence(s_state.current, target)) {
      /* A consumed target can coexist with an older stored snooze after a
       * returned write error. Completion must not revive that snooze. */
      save = clear_snooze();
      s_state.phase = WAC_ALARM_IDLE;
      s_state.current = (wac_alarm_occurrence_t){0};
    }
  } else if (s_one_time.date) {
    s_one_time = (stored_occurrence_t){0};
    s_target_dirty = true;
    s_complete_pending = false;
  }
  if (s_arm_next_pending) {
    /* Remember idle arming across restart without marking a captured ringing
     * alarm handled. Preserve missed notices and leave exactly due eligible. */
    if (!one_time && schedule.previous.valid && schedule.previous.utc < clock->utc_now) {
      save = s_state.phase == WAC_ALARM_IDLE;
      remember(schedule.previous, save);
    }
    s_arm_next_pending = false;
  }
  s_state.next = schedule.next;
  if (!one_time && s_state.next.valid && handled(s_state.next)) {
    time_t after = s_handled.utc > s_state.next.utc ? s_handled.utc : s_state.next.utc;
    wac_alarm_schedule_t future;
    wac_alarm_schedule_calculate(config, after, &future);
    s_state.next = future.next;
  }
  if (s_state.phase == WAC_ALARM_SNOOZED && clock->utc_now >= s_state.snooze_deadline) {
    if (difftime(clock->utc_now, s_state.snooze_deadline) <= CATCHUP_SECONDS)
      save = start_ring(s_state.current);
    else {
      consume_one_time(s_state.current);
      save = clear_snooze();
      s_state.phase = WAC_ALARM_IDLE;
      s_state.current = (wac_alarm_occurrence_t){0};
    }
  }
  if (s_state.phase == WAC_ALARM_IDLE && schedule.previous.valid
      && (!one_time ? !handled(schedule.previous) : !s_one_time.reserved)) {
    if (difftime(clock->utc_now, schedule.previous.utc) <= CATCHUP_SECONDS)
      save = start_ring(schedule.previous) || save;
    else {
      remember(schedule.previous, true);
      consume_one_time(schedule.previous);
      /* Retain the pending notice until acknowledged or an alarm rings. */
      if (!s_state.missed.valid) {
        s_state.missed = schedule.previous;
        s_record.missed = store_occurrence(schedule.previous);
      }
      save = true;
    }
  }
  publish();
  return save || s_target_dirty || s_complete_pending ? save_changes(save) : ESP_OK;
}
esp_err_t wac_alarm_snooze(const device_config_t *config, const wac_time_snapshot_t *clock)
{
  if (!config || !clock) return ESP_ERR_INVALID_ARG;
  if (!s_initialized || !clock->usable || s_state.phase != WAC_ALARM_RINGING)
    return ESP_ERR_INVALID_STATE;
  if (config->alarm_snooze_minutes != 5 && config->alarm_snooze_minutes != 10
      && config->alarm_snooze_minutes != 15) return ESP_ERR_INVALID_ARG;
  s_state.phase = WAC_ALARM_SNOOZED;
  s_state.snooze_deadline = clock->utc_now + config->alarm_snooze_minutes * 60;
  s_record.snooze = store_occurrence(s_state.current);
  s_record.snooze_deadline = s_state.snooze_deadline;
  remember(s_state.current, true);
  publish();
  return save_changes(true);
}
esp_err_t wac_alarm_dismiss(void)
{
  if (!s_initialized || s_state.phase == WAC_ALARM_IDLE) return ESP_ERR_INVALID_STATE;
  remember(s_state.current, true);
  consume_one_time(s_state.current);
  clear_snooze();
  s_state.phase = WAC_ALARM_IDLE;
  s_state.current = (wac_alarm_occurrence_t){0};
  publish();
  return save_changes(true);
}
esp_err_t wac_alarm_dismiss_missed(void)
{
  if (!s_initialized || !s_state.missed.valid) return ESP_ERR_INVALID_STATE;
  s_state.missed = (wac_alarm_occurrence_t){0};
  s_record.missed = (stored_occurrence_t){0};
  publish();
  return save_changes(true);
}
