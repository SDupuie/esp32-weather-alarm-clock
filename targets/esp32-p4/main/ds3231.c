#include "ds3231.h"

#include <stddef.h>
#include <stdint.h>

#include "bsp/esp32_p4_wifi6_touch_lcd_xc.h"
#include "driver/i2c_master.h"

/* DS3231 Rev. 10, pp. 11–14:
 * https://www.analog.com/media/en/technical-documentation/data-sheets/DS3231.pdf
 * Calendar compensation is guaranteed through 2099. The century bit is rejected
 * rather than claiming correct hardware leap-year behavior beyond that range. */
#define DS3231_ADDRESS 0x68
#define DS3231_TIMEOUT_MS 100
#define DS3231_CONTROL 0x0e
#define DS3231_STATUS 0x0f
#define DS3231_EOSC 0x80
#define DS3231_OSF 0x80
#define DS3231_EN32KHZ 0x08
#define DS3231_ALARM_FLAGS 0x03
#define UTC_2000 INT64_C(946684800)
#define UTC_2100 INT64_C(4102444800)

static i2c_master_dev_handle_t rtc;

static esp_err_t read_registers(uint8_t first, uint8_t *values, size_t count)
{
  if (rtc == NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  return i2c_master_transmit_receive(rtc, &first, 1, values, count,
                                     DS3231_TIMEOUT_MS);
}

static esp_err_t write_register(uint8_t reg, uint8_t value)
{
  const uint8_t bytes[] = {reg, value};
  return i2c_master_transmit(rtc, bytes, sizeof(bytes), DS3231_TIMEOUT_MS);
}

static esp_err_t enable_oscillator(void)
{
  uint8_t control;
  esp_err_t err = read_registers(DS3231_CONTROL, &control, 1);
  if (err != ESP_OK || (control & DS3231_EOSC) == 0) {
    return err;
  }
  /* EOSC=1 stops the oscillator on battery power; preserve the other controls. */
  return write_register(DS3231_CONTROL, control & ~DS3231_EOSC);
}

esp_err_t wac_ds3231_init(void)
{
  if (rtc == NULL) {
    esp_err_t err = bsp_i2c_init();
    if (err != ESP_OK) {
      return err;
    }
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (bus == NULL) {
      return ESP_ERR_INVALID_STATE;
    }
    const i2c_device_config_t config = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = DS3231_ADDRESS,
      .scl_speed_hz = 100000,
    };
    i2c_master_dev_handle_t device = NULL;
    err = i2c_master_bus_add_device(bus, &config, &device);
    if (err != ESP_OK) {
      return err;
    }
    rtc = device;
  }
  return enable_oscillator();
}

static bool decode_bcd(uint8_t value, unsigned max, unsigned *out)
{
  unsigned units = value & 0x0f;
  unsigned tens = value >> 4;
  if (units > 9 || tens > 9 || tens * 10 + units > max) {
    return false;
  }
  *out = tens * 10 + units;
  return true;
}

static uint8_t encode_bcd(unsigned value)
{
  return (uint8_t)((value / 10) * 16 + value % 10);
}

static unsigned days_in_month(unsigned year, unsigned month)
{
  static const uint8_t lengths[] = {31, 28, 31, 30, 31, 30,
                                   31, 31, 30, 31, 30, 31};
  /* All accepted years are 2000–2099, so division by four is sufficient. */
  return lengths[month - 1] + (month == 2 && year % 4 == 0);
}

esp_err_t wac_ds3231_read(time_t *utc, bool *oscillator_stopped)
{
  if (utc == NULL || oscillator_stopped == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  *oscillator_stopped = false;
  uint8_t registers[DS3231_STATUS + 1];
  esp_err_t err = read_registers(0, registers, sizeof(registers));
  if (err != ESP_OK) {
    return err;
  }
  /* A single burst uses the RTC's buffered calendar, including across rollover.
   * Preserve observed lost-time evidence even if the calendar cannot be used. */
  *oscillator_stopped = (registers[DS3231_STATUS] & DS3231_OSF) != 0;

  unsigned second, minute, hour, day, month, year;
  if ((registers[0] & 0x80) != 0 ||
      (registers[1] & 0x80) != 0 ||
      (registers[2] & 0x80) != 0 ||
      (registers[3] & 0xf8) != 0 || registers[3] < 1 || registers[3] > 7 ||
      (registers[4] & 0xc0) != 0 ||
      (registers[5] & 0xe0) != 0 ||
      !decode_bcd(registers[0], 59, &second) ||
      !decode_bcd(registers[1], 59, &minute) ||
      !decode_bcd(registers[4], 31, &day) || day == 0 ||
      !decode_bcd(registers[5], 12, &month) || month == 0 ||
      !decode_bcd(registers[6], 99, &year)) {
    return ESP_ERR_INVALID_RESPONSE;
  }
  if ((registers[2] & 0x40) != 0) {
    if (!decode_bcd(registers[2] & 0x1f, 12, &hour) || hour == 0) {
      return ESP_ERR_INVALID_RESPONSE;
    }
    hour = hour % 12 + ((registers[2] & 0x20) != 0 ? 12 : 0);
  } else if (!decode_bcd(registers[2], 23, &hour)) {
    return ESP_ERR_INVALID_RESPONSE;
  }
  year += 2000;
  if (day > days_in_month(year, month)) {
    return ESP_ERR_INVALID_RESPONSE;
  }

  /* Integer UTC conversion deliberately avoids the process's local TZ rules. */
  int64_t days = 0;
  for (unsigned y = 2000; y < year; ++y) {
    days += 365 + (y % 4 == 0);
  }
  for (unsigned m = 1; m < month; ++m) {
    days += days_in_month(year, m);
  }
  days += day - 1;
  int64_t seconds = UTC_2000 + days * 86400 + hour * 3600 + minute * 60 + second;
  time_t result = (time_t)seconds;
  if ((int64_t)result != seconds) {
    return ESP_ERR_INVALID_RESPONSE;
  }
  *utc = result;
  return ESP_OK;
}

esp_err_t wac_ds3231_write(time_t utc)
{
  int64_t seconds = (int64_t)utc;
  if (seconds < UTC_2000 || seconds >= UTC_2100) {
    return ESP_ERR_INVALID_ARG;
  }
  if (rtc == NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  int64_t elapsed = seconds - UTC_2000;
  unsigned days = (unsigned)(elapsed / 86400);
  unsigned day_of_week = (days + 6) % 7 + 1; /* 2000-01-01 was Saturday; Sunday=1. */
  unsigned year = 2000;
  while (days >= 365U + (year % 4 == 0)) {
    days -= 365U + (year % 4 == 0);
    ++year;
  }
  unsigned month = 1;
  while (days >= days_in_month(year, month)) {
    days -= days_in_month(year, month);
    ++month;
  }
  unsigned within_day = (unsigned)(elapsed % 86400);
  const uint8_t calendar[] = {
    0,
    encode_bcd(within_day % 60),
    encode_bcd((within_day / 60) % 60),
    encode_bcd(within_day / 3600),
    (uint8_t)day_of_week,
    encode_bcd(days + 1),
    encode_bcd(month),
    encode_bcd(year - 2000),
  };
  esp_err_t err = enable_oscillator();
  if (err != ESP_OK) {
    return err;
  }
  /* Writing seconds resets the countdown chain. One finite burst writes all
   * seven calendar bytes well within the datasheet's one-second requirement. */
  err = i2c_master_transmit(rtc, calendar, sizeof(calendar), DS3231_TIMEOUT_MS);
  if (err != ESP_OK) {
    return err;
  }
  uint8_t status;
  err = read_registers(DS3231_STATUS, &status, 1);
  if (err != ESP_OK) {
    return err;
  }
  /* A1F/A2F are cleared by writing zero. Write ones to preserve them, including
   * a flag set after the read; preserve EN32kHz and clear only OSF. BSY is read-only. */
  return write_register(DS3231_STATUS,
                        (status & DS3231_EN32KHZ) | DS3231_ALARM_FLAGS);
}
