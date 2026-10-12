#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "solar_times.h"

/* Independent US Naval Observatory results retrieved 2026-10-11, rounded by
 * the provider to a minute. Each query is reproducible using this template:
 * https://aa.usno.navy.mil/api/rstt/oneday?date=YYYY-MM-DD&coords=LAT,LON&tz=OFFSET
 * Times below are local, with the explicit UTC offset shown (dst omitted).
 * https://aa.usno.navy.mil/data/api.html
 * The 120-second tolerance allows provider rounding and differences between
 * the two solar models; this is not a claim of observed horizon accuracy. */
typedef struct {
  int year, month, day;
  double latitude, longitude;
  int offset_hours;
  int rise_hour, rise_minute, set_hour, set_minute;
} reference_t;

static const reference_t references[] = {
  {2026, 10, 11, 30.2672, -97.7431, -5, 7, 31, 19, 4}, /* Austin autumn */
  {2026, 6, 21, 30.2672, -97.7431, -5, 6, 30, 20, 36}, /* Austin summer */
  {2026, 12, 21, 30.2672, -97.7431, -6, 7, 23, 17, 35}, /* Austin winter */
  {2026, 10, 11, -36.8485, 174.7633, 13, 6, 43, 19, 33}, /* Auckland */
  {2026, 10, 11, 1.8721, -157.4278, 14, 6, 14, 18, 19}, /* Kiritimati */
  {2024, 2, 29, -33.8688, 151.2093, 11, 6, 42, 19, 33}, /* Sydney leap day */
  {2026, 1, 1, 1.8721, -157.4278, 14, 6, 32, 18, 34}, /* Date/year boundary */
};

static time_t at(const reference_t *r, int hour, int minute)
{
  struct tm date = {.tm_year = r->year - 1900, .tm_mon = r->month - 1,
    .tm_mday = r->day, .tm_hour = hour, .tm_min = minute};
  return timegm(&date) - r->offset_hours * 3600;
}

static void reference_checks(void)
{
  double maximum_error = 0;
  for (size_t i = 0; i < sizeof(references) / sizeof(references[0]); ++i) {
    const reference_t *r = &references[i];
    solar_times_t events;
    solar_times_calculate(at(r, 12, 0), r->latitude, r->longitude, &events);
    assert(events.available && events.sunrise < events.sunset);
    double rise_error = fabs(difftime(events.sunrise, at(r, r->rise_hour, r->rise_minute)));
    double set_error = fabs(difftime(events.sunset, at(r, r->set_hour, r->set_minute)));
    assert(rise_error <= 120 && set_error <= 120);
    maximum_error = fmax(maximum_error, fmax(rise_error, set_error));
    /* The same anchor must select the same solar day throughout its nearby
     * noon window. This protects the caller's ordinary noon conversion. */
    solar_times_t shifted;
    solar_times_calculate(at(r, 12, 0) + 3600, r->latitude, r->longitude, &shifted);
    assert(shifted.available && shifted.sunrise == events.sunrise
      && shifted.sunset == events.sunset);
  }
  printf("Independent USNO references: 7 locations/dates, maximum event difference %.0f seconds.\n",
    maximum_error);
}

static void assert_unavailable(time_t noon, double latitude, double longitude)
{
  solar_times_t events = {.available = true, .sunrise = 123, .sunset = 456};
  solar_times_calculate(noon, latitude, longitude, &events);
  assert(!events.available && events.sunrise == 0 && events.sunset == 0);
}

static void unavailable_checks(void)
{
  time_t noon = at(&references[0], 12, 0);
  const double invalid[] = {NAN, INFINITY, -INFINITY};
  for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
    assert_unavailable(noon, invalid[i], 0);
    assert_unavailable(noon, 0, invalid[i]);
  }
  assert_unavailable(noon, 90.01, 0); assert_unavailable(noon, -90.01, 0);
  assert_unavailable(noon, 0, 180.01); assert_unavailable(noon, 0, -180.01);
  assert_unavailable(noon, 90, 0); assert_unavailable(noon, -90, 0);
  /* USNO confirms continuous daylight/night for these Tromso dates:
   * ?date=2026-06-21&coords=69.6492,18.9553&tz=2
   * ?date=2026-12-21&coords=69.6492,18.9553&tz=1 */
  reference_t polar = {.year = 2026, .month = 6, .day = 21,
    .latitude = 69.6492, .longitude = 18.9553, .offset_hours = 2};
  assert_unavailable(at(&polar, 12, 0), polar.latitude, polar.longitude);
  polar.month = 12; polar.offset_hours = 1;
  assert_unavailable(at(&polar, 12, 0), polar.latitude, polar.longitude);
  solar_times_calculate(noon, 0, 0, NULL);
  solar_times_t equator, east, west;
  solar_times_calculate(noon, 0, 0, &equator); assert(equator.available);
  solar_times_calculate(noon, 0, 180, &east);
  solar_times_calculate(noon, 0, -180, &west);
  assert(east.available && west.available && east.sunrise == west.sunrise
    && east.sunset == west.sunset);
}

static void timezone_independence_checks(void)
{
  const reference_t *r = &references[4];
  time_t noon = at(r, 12, 0);
  solar_times_t original;
  solar_times_calculate(noon, r->latitude, r->longitude, &original);
  const char *zones[] = {"UTC0", "CST6CDT,M3.2.0,M11.1.0", "NZST-12NZDT,M9.5.0,M4.1.0/3"};
  for (size_t i = 0; i < sizeof(zones) / sizeof(zones[0]); ++i) {
    assert(setenv("TZ", zones[i], 1) == 0); tzset();
    solar_times_t events;
    solar_times_calculate(noon, r->latitude, r->longitude, &events);
    assert(events.available && events.sunrise == original.sunrise
      && events.sunset == original.sunset);
  }
}

int main(void)
{
  reference_checks(); unavailable_checks(); timezone_independence_checks();
  puts("Production solar checks passed: independent reference times, UTC dates, leap day, southern hemisphere, unavailable events and timezone independence.");
  return 0;
}
