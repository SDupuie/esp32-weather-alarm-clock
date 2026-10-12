#include "solar_times.h"

#include <math.h>
#include <limits.h>

#define SECONDS_PER_DAY 86400.0
#define UNIX_EPOCH_JD 2440587.5
#define DEGREES_TO_RADIANS 0.01745329251994329577

/* The NOAA solar calculator's Meeus equations, restricted to the quantities
 * needed for sunrise and sunset. No solar position, timezone or polar-season
 * search machinery is needed by Night Shift.
 * https://gml.noaa.gov/grad/solcalc/main.js
 * https://gml.noaa.gov/grad/solcalc/calcdetails.html */
static void solar_position(double jd, double *declination, double *equation)
{
  double t = (jd - 2451545.0) / 36525.0;
  double mean_longitude = fmod(280.46646 + t * (36000.76983 + t * 0.0003032), 360.0);
  double anomaly = (357.52911 + t * (35999.05029 - 0.0001537 * t))
    * DEGREES_TO_RADIANS;
  double eccentricity = 0.016708634 - t * (0.000042037 + 0.0000001267 * t);
  double center = sin(anomaly) * (1.914602 - t * (0.004817 + 0.000014 * t))
    + sin(2.0 * anomaly) * (0.019993 - 0.000101 * t)
    + sin(3.0 * anomaly) * 0.000289;
  double omega = (125.04 - 1934.136 * t) * DEGREES_TO_RADIANS;
  double apparent_longitude = (mean_longitude + center - 0.00569
    - 0.00478 * sin(omega)) * DEGREES_TO_RADIANS;
  double obliquity = (23.0 + (26.0 + (21.448
    - t * (46.8150 + t * (0.00059 - t * 0.001813))) / 60.0) / 60.0
    + 0.00256 * cos(omega)) * DEGREES_TO_RADIANS;
  *declination = asin(sin(obliquity) * sin(apparent_longitude));
  double y = tan(obliquity / 2.0);
  y *= y;
  double l = mean_longitude * DEGREES_TO_RADIANS;
  *equation = (y * sin(2.0 * l) - 2.0 * eccentricity * sin(anomaly)
    + 4.0 * eccentricity * y * sin(anomaly) * cos(2.0 * l)
    - 0.5 * y * y * sin(4.0 * l)
    - 1.25 * eccentricity * eccentricity * sin(2.0 * anomaly))
    * 4.0 / DEGREES_TO_RADIANS;
}

static bool event_minutes(double jd, double latitude, double longitude,
                          bool sunrise, double *minutes)
{
  double declination, equation;
  solar_position(jd, &declination, &equation);
  double lat = latitude * DEGREES_TO_RADIANS;
  double denominator = cos(lat) * cos(declination);
  if (fabs(denominator) < 1e-12) return false;
  /* 90.833 degrees accounts for the apparent solar disk and average
   * atmospheric refraction. An out-of-range cosine means no such event;
   * clamping it would invent a sunrise or sunset on a polar day. */
  double cosine = (cos(90.833 * DEGREES_TO_RADIANS)
    - sin(lat) * sin(declination)) / denominator;
  if (!isfinite(cosine) || cosine < -1.0 || cosine > 1.0) return false;
  double angle = acos(cosine) / DEGREES_TO_RADIANS;
  *minutes = 720.0 - 4.0 * (longitude + (sunrise ? angle : -angle)) - equation;
  return isfinite(*minutes);
}

void solar_times_calculate(time_t utc_noon, double latitude, double longitude,
                           solar_times_t *out)
{
  if (out == NULL) return;
  *out = (solar_times_t){0};
  if (!isfinite(latitude) || latitude < -90.0 || latitude > 90.0
      || !isfinite(longitude) || longitude < -180.0 || longitude > 180.0)
    return;

  double utc_day = floor((double) utc_noon / SECONDS_PER_DAY);
  double jd = utc_day + UNIX_EPOCH_JD;
  double declination, equation;
  solar_position(jd, &declination, &equation);
  double noon = 720.0 - 4.0 * longitude - equation;
  /* The date-line timezone can differ from the longitude's natural UTC day.
   * Choose the solar day by the supplied local-noon anchor, rather than
   * wrapping event minutes into a UTC clock time and losing their date. */
  double day_shift = round(((double) utc_noon - utc_day * SECONDS_PER_DAY
    - noon * 60.0) / SECONDS_PER_DAY);
  jd += day_shift;
  utc_day += day_shift;

  double rise, set;
  if (!event_minutes(jd, latitude, longitude, true, &rise)
      || !event_minutes(jd, latitude, longitude, false, &set)
      || !event_minutes(jd + rise / 1440.0, latitude, longitude, true, &rise)
      || !event_minutes(jd + set / 1440.0, latitude, longitude, false, &set))
    return;
  double rise_seconds = round(utc_day * SECONDS_PER_DAY + rise * 60.0);
  double set_seconds = round(utc_day * SECONDS_PER_DAY + set * 60.0);
  /* ESP-IDF uses signed 64-bit time_t. These bounds also keep extreme invalid
   * anchors from reaching an out-of-range integer conversion. */
  double time_limit = ldexp(1.0, sizeof(time_t) * CHAR_BIT - 1);
  if (!isfinite(rise_seconds) || !isfinite(set_seconds)
      || rise_seconds < -time_limit || rise_seconds >= time_limit
      || set_seconds < -time_limit || set_seconds >= time_limit)
    return;
  out->sunrise = (time_t) rise_seconds;
  out->sunset = (time_t) set_seconds;
  out->available = true;
}
