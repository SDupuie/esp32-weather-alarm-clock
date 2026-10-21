#include "forecast_representative.h"

#include <ctype.h>
#include <math.h>
#include <string.h>

typedef struct {
  int samples, hail, thunder, lightning, thundersnow, wet, rain, showers, snow, mixed;
  int freezing, fog, cloudy, partly, clear, cloud_samples;
  double precipitation, snowfall, probability, cloud;
} daytime_summary_t;

static const cJSON *array_item(const cJSON *hourly, const char *key, int i)
{
  return cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(hourly, key), i);
}

static double number_at(const cJSON *hourly, const char *key, int i)
{
  const cJSON *v = array_item(hourly, key, i);
  return cJSON_IsNumber(v) && isfinite(v->valuedouble) ? v->valuedouble : NAN;
}

static bool same_hour(const cJSON *hourly, int i, int j)
{
  /* An epoch distinguishes the two occurrences of a repeated local hour at a
   * fall clock change. Wall-time strings are the fallback identity only. */
  double a = number_at(hourly, "epoch", i), b = number_at(hourly, "epoch", j);
  if (isfinite(a) && isfinite(b)) return a == b;
  const char *ta = cJSON_GetStringValue(array_item(hourly, "time", i));
  const char *tb = cJSON_GetStringValue(array_item(hourly, "time", j));
  return ta && tb && strcmp(ta, tb) == 0;
}

static bool daytime(const char *time, const char *date)
{
  if (!time || !date || strlen(time) < 16 || strlen(date) < 10
      || strncmp(time, date, 10) || time[10] != 'T'
      || !isdigit((unsigned char)time[11]) || !isdigit((unsigned char)time[12])) return false;
  int hour = (time[11]-'0')*10 + time[12]-'0';
  return hour >= 8 && hour <= 20;
}

static daytime_summary_t summarize(const cJSON *hourly, const char *date)
{
  daytime_summary_t s = {0};
  const cJSON *times = cJSON_GetObjectItemCaseSensitive(hourly, "time");
  for (int i = 0; i < cJSON_GetArraySize(times); ++i) {
    const char *time = cJSON_GetStringValue(cJSON_GetArrayItem(times, i));
    if (!daytime(time, date)) continue;
    bool duplicate = false;
    for (int j = 0; j < i; ++j) {
      if (same_hour(hourly, i, j)) { duplicate = true; break; }
    }
    if (duplicate) continue;
    uint32_t f = weather_hourly_flags_at(hourly, i);
    weather_sky_t sky = weather_hourly_sky_at(hourly, i);
    double precipitation = number_at(hourly, "precipitation", i);
    double probability = number_at(hourly, "precipitation_probability", i);
    double rain = number_at(hourly, "rain", i), showers = number_at(hourly, "showers", i);
    double snowfall = number_at(hourly, "snowfall", i), cloud = number_at(hourly, "cloud_cover", i);
    bool snow_hour = (f & WEATHER_FACT_SNOW) || snowfall > 0;
    bool liquid = (f & WEATHER_FACT_LIQUID) || rain > 0 || showers > 0;
    bool falling_snow = (f & WEATHER_FACT_FALLING_SNOW) || snowfall > 0;
    s.samples++;
    s.hail += !!(f & WEATHER_FACT_HAIL);
    s.thunder += !!(f & WEATHER_FACT_THUNDER);
    s.lightning += !!(f & WEATHER_FACT_LIGHTNING) && !(f & WEATHER_FACT_THUNDER);
    s.thundersnow += !!(f & WEATHER_FACT_THUNDER) && falling_snow;
    s.snow += snow_hour;
    s.mixed += snow_hour && liquid && falling_snow;
    s.freezing += !!(f & WEATHER_FACT_FREEZING);
    s.fog += !!(f & WEATHER_FACT_FOG);
    s.wet += (liquid && (!(f & WEATHER_FACT_FREEZING) || (f & WEATHER_FACT_DRIZZLE)))
      || precipitation > 0 || rain > 0 || showers > 0;
    s.showers += !!(f & (WEATHER_FACT_SHOWERS | WEATHER_FACT_DRIZZLE)) || showers > 0;
    s.rain += (liquid && !(f & (WEATHER_FACT_SHOWERS | WEATHER_FACT_DRIZZLE))) || rain > 0;
    if (sky == WEATHER_SKY_CLOUDY || cloud >= 75) s.cloudy++;
    else if (sky == WEATHER_SKY_PARTLY || cloud >= 35) s.partly++;
    else if (sky == WEATHER_SKY_CLEAR || (isfinite(cloud) && cloud < 35)) s.clear++;
    if (isfinite(precipitation) && precipitation > 0) s.precipitation += precipitation;
    if (isfinite(snowfall) && snowfall > 0) s.snowfall += snowfall;
    if (isfinite(probability) && probability > s.probability) s.probability = probability;
    if (isfinite(cloud)) { s.cloud += cloud; s.cloud_samples++; }
  }
  if (s.cloud_samples) s.cloud /= s.cloud_samples;
  return s;
}

weather_picture_t forecast_pick_representative_picture(const cJSON *hourly,
  const char *date, const weather_condition_facts_t *daily_facts,
  double daily_high, double daily_low, int snow_threshold)
{
  daytime_summary_t s = summarize(hourly, date);
  bool cold = (daily_high + daily_low) / 2.0 <= snow_threshold;
  if (!s.samples) {
    if (!daily_facts) return WEATHER_PICTURE_NONE;
    weather_condition_facts_t f = *daily_facts;
    f.flags &= ~(WEATHER_FACT_TORNADO | WEATHER_FACT_HAIL | WEATHER_FACT_ICE
      | WEATHER_FACT_WIND | WEATHER_FACT_MIXED);
    /* Day-wide evidence cannot establish thundersnow. Keep its ordinary family. */
    if (f.flags & WEATHER_FACT_THUNDER) f.flags &= ~WEATHER_FACT_FALLING_SNOW;
    weather_picture_t picture = weather_picture_for_facts(&f);
    if (picture == WEATHER_PICTURE_THUNDER && (f.flags & WEATHER_FACT_THUNDER) && cold) return WEATHER_PICTURE_THUNDERSNOW;
    return picture;
  }
  if (s.hail >= 2) return WEATHER_PICTURE_HAIL;
  int electrical_hours = s.thunder + s.lightning;
  if ((electrical_hours >= 2 && (s.wet >= 2 || s.precipitation >= 0.5 || s.probability >= 35))
      || (electrical_hours >= 1 && (s.wet >= 1 || s.precipitation > 0) && s.probability >= 60)) {
    return s.thundersnow || (s.thunder && cold) ? WEATHER_PICTURE_THUNDERSNOW : WEATHER_PICTURE_THUNDER;
  }
  if (s.freezing >= 1 && (s.wet >= 2 || s.precipitation >= 1 || s.probability >= 60)) return WEATHER_PICTURE_FREEZING_RAIN;
  if (s.snow >= 2 || s.snowfall >= 0.5 || (cold && s.wet >= 3 && s.probability >= 60)) {
    return s.snow && s.mixed * 2 > s.snow ? WEATHER_PICTURE_RAIN_SNOW : WEATHER_PICTURE_SNOW;
  }
  if (s.probability >= 50 && s.precipitation >= 1.5) return s.showers >= s.rain ? WEATHER_PICTURE_SHOWERS : WEATHER_PICTURE_RAIN;
  if (s.fog >= 3 && s.wet <= 1 && s.cloud_samples && s.cloud >= 70) return WEATHER_PICTURE_FOG;
  int maximum = s.clear > s.partly ? s.clear : s.partly;
  if ((s.cloud_samples && s.cloud >= 70) || (s.cloudy > 0 && s.cloudy >= maximum)) return WEATHER_PICTURE_CLOUDY;
  if ((s.cloud_samples && s.cloud >= 35) || (s.partly > 0 && s.partly >= s.clear)) return WEATHER_PICTURE_PARTLY;
  return s.clear > 0 ? WEATHER_PICTURE_CLEAR : WEATHER_PICTURE_NONE;
}
