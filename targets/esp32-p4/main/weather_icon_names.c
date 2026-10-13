#include "weather_icon_names.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const char *const WEATHER_ICON_NAMES[] = {
  "clear-day", "clear-night", "cloudy", "fog", "partlycloudy-day",
  "partlycloudy-night", "rain", "showers-day", "showers-night", "sleet",
  "snow", "thunderstorm", "thundersnow", "freezing-rain", "rain-snow",
  "hail", "ice", "wind", "tornado"
};

static bool equal(const char *a, const char *b)
{
  if (!a || !b) return false;
  while (*a && *b) {
    if (tolower((unsigned char)*a++) != tolower((unsigned char)*b++)) return false;
  }
  return *a == *b;
}

const char *weather_icon_fallback_name(void) { return ""; }

bool weather_icon_name_is_known(const char *name)
{
  for (size_t i = 0; i < sizeof(WEATHER_ICON_NAMES)/sizeof(WEATHER_ICON_NAMES[0]); ++i) {
    if (equal(name, WEATHER_ICON_NAMES[i])) return true;
  }
  return false;
}

const char *weather_icon_name_or_fallback(const char *name)
{
  return weather_icon_name_is_known(name) ? name : weather_icon_fallback_name();
}

static void finish_compounds(weather_condition_facts_t *f)
{
  if ((f->flags & WEATHER_FACT_LIQUID) && (f->flags & WEATHER_FACT_FALLING_SNOW)) {
    f->flags |= WEATHER_FACT_MIXED;
  }
}

weather_condition_facts_t weather_facts_from_open_meteo(int code, double rain,
  double showers, double snow)
{
  weather_condition_facts_t f = {0};
  switch (code) {
    case 0: f.sky = WEATHER_SKY_CLEAR; break;
    case 1: f.sky = WEATHER_SKY_PARTLY; f.mostly_clear = true; break;
    case 2: f.sky = WEATHER_SKY_PARTLY; break;
    case 3: f.sky = WEATHER_SKY_CLOUDY; break;
    case 45: case 48:
      f.flags |= WEATHER_FACT_FOG;
      f.fog_caption = code == 48 ? "Freezing Fog" : "Fog"; break;
    case 51: case 53: case 55:
      f.flags |= WEATHER_FACT_LIQUID | WEATHER_FACT_DRIZZLE;
      f.liquid_caption = code == 51 ? "Light Drizzle" : code == 55 ? "Heavy Drizzle" : "Drizzle"; break;
    case 56: case 57:
      f.flags |= WEATHER_FACT_FREEZING | WEATHER_FACT_LIQUID | WEATHER_FACT_DRIZZLE;
      f.freezing_caption = code == 56 ? "Light Freezing Drizzle" : "Heavy Freezing Drizzle"; break;
    case 61: case 63: case 65:
      f.flags |= WEATHER_FACT_LIQUID;
      f.liquid_caption = code == 61 ? "Light Rain" : code == 65 ? "Heavy Rain" : "Rain"; break;
    case 66: case 67:
      f.flags |= WEATHER_FACT_FREEZING | WEATHER_FACT_LIQUID;
      f.freezing_caption = code == 66 ? "Light Freezing Rain" : "Heavy Freezing Rain"; break;
    case 71: case 73: case 75:
      f.flags |= WEATHER_FACT_SNOW | WEATHER_FACT_FALLING_SNOW;
      f.snow_caption = code == 71 ? "Light Snow" : code == 75 ? "Heavy Snow" : "Snow"; break;
    case 77: f.flags |= WEATHER_FACT_SNOW; f.snow_caption = "Snow Grains"; break;
    case 80: case 81: case 82:
      f.flags |= WEATHER_FACT_LIQUID | WEATHER_FACT_SHOWERS;
      f.liquid_caption = code == 80 ? "Light Showers" : code == 82 ? "Heavy Showers" : "Showers"; break;
    case 85: case 86:
      f.flags |= WEATHER_FACT_SNOW | WEATHER_FACT_FALLING_SNOW;
      f.snow_caption = code == 85 ? "Light Snow Showers" : "Heavy Snow Showers"; break;
    case 96: case 99: f.flags |= WEATHER_FACT_HAIL; /* fall through */
    case 95: case 97: f.flags |= WEATHER_FACT_THUNDER; break;
    default: break;
  }
  if ((isfinite(rain) && rain > 0) || (isfinite(showers) && showers > 0)) f.flags |= WEATHER_FACT_LIQUID;
  if (isfinite(showers) && showers > 0) f.flags |= WEATHER_FACT_SHOWERS;
  if (isfinite(snow) && snow > 0) f.flags |= WEATHER_FACT_SNOW | WEATHER_FACT_FALLING_SNOW;
  finish_compounds(&f);
  return f;
}

static void add_vc_id(weather_condition_facts_t *f, int id)
{
  switch (id) {
    case 1: f->flags |= WEATHER_FACT_SNOW; f->snow_caption = "Blowing / Drifting Snow"; break;
    case 2: case 3: case 4:
      f->flags |= WEATHER_FACT_LIQUID | WEATHER_FACT_DRIZZLE;
      f->liquid_caption = id == 3 ? "Heavy Drizzle" : id == 4 ? "Light Drizzle" : "Drizzle"; break;
    case 5: case 6:
      f->flags |= WEATHER_FACT_LIQUID | WEATHER_FACT_DRIZZLE;
      f->liquid_caption = id == 5 ? "Heavy Drizzle & Rain" : "Light Drizzle & Rain"; break;
    case 7: f->flags |= WEATHER_FACT_FOG; f->fog_caption = "Dust Storm"; break;
    case 8: case 12: case 19:
      f->flags |= WEATHER_FACT_FOG;
      f->fog_caption = id == 12 ? "Freezing Fog" : id == 19 ? "Mist" : "Fog"; break;
    case 9: case 10: case 11:
      f->flags |= WEATHER_FACT_FREEZING | WEATHER_FACT_LIQUID;
      f->freezing_caption = id == 10 ? "Heavy Freezing Precipitation" : id == 11 ? "Light Freezing Precipitation" : "Freezing Precipitation"; break;
    case 13: case 14:
      f->flags |= WEATHER_FACT_FREEZING | WEATHER_FACT_LIQUID;
      f->freezing_caption = id == 13 ? "Heavy Freezing Rain" : "Light Freezing Rain"; break;
    case 15: f->flags |= WEATHER_FACT_TORNADO; break;
    case 16: case 40: f->flags |= WEATHER_FACT_HAIL; f->hail_caption = id == 16 ? "Hail Showers" : "Hail"; break;
    case 17: f->flags |= WEATHER_FACT_ICE; break;
    case 18: f->flags |= WEATHER_FACT_LIGHTNING; break;
    case 21: case 25: case 26:
      f->flags |= WEATHER_FACT_LIQUID;
      f->liquid_caption = id == 25 ? "Heavy Rain" : id == 26 ? "Light Rain" : "Rain"; break;
    case 22: case 23: case 32:
      f->flags |= WEATHER_FACT_LIQUID | WEATHER_FACT_SNOW | WEATHER_FACT_FALLING_SNOW | WEATHER_FACT_MIXED;
      f->mixed_caption = id == 22 ? "Heavy Rain & Snow" : id == 23 ? "Light Rain & Snow" : "Rain & Snow Showers";
      break;
    case 24: f->flags |= WEATHER_FACT_LIQUID | WEATHER_FACT_SHOWERS; f->liquid_caption = "Showers"; break;
    case 30: f->flags |= WEATHER_FACT_FOG; f->fog_caption = "Smoke / Haze"; break;
    case 31: case 34: case 35:
      f->flags |= WEATHER_FACT_SNOW | WEATHER_FACT_FALLING_SNOW;
      f->snow_caption = id == 34 ? "Heavy Snow" : id == 35 ? "Light Snow" : "Snow"; break;
    case 33:
      f->flags |= WEATHER_FACT_SNOW | WEATHER_FACT_FALLING_SNOW;
      f->snow_caption = "Snow Showers"; break;
    case 36: f->flags |= WEATHER_FACT_WIND; f->wind_caption = "Squalls"; break;
    case 37: case 38: f->flags |= WEATHER_FACT_THUNDER; break;
    case 39: f->flags |= WEATHER_FACT_SNOW; f->snow_caption = "Diamond Dust / Ice Crystals"; break;
    case 41: f->sky = WEATHER_SKY_CLOUDY; break;
    case 42: f->sky = WEATHER_SKY_PARTLY; break;
    case 43: f->sky = WEATHER_SKY_CLEAR; break;
    /* Vicinity precipitation and coverage trends do not establish local weather. */
    default: break;
  }
}

weather_condition_facts_t weather_facts_from_visual_crossing(const cJSON *record, double snow)
{
  weather_condition_facts_t f = {0};
  bool dry_thunder = false;
  const char *ids = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(record, "conditions"));
  if (ids) {
    while (*ids) {
      while (isspace((unsigned char)*ids) || *ids == ',') ++ids;
      const char *end = strchr(ids, ',');
      if (!end) end = ids + strlen(ids);
      const char *trim = end;
      while (trim > ids && isspace((unsigned char)trim[-1])) --trim;
      if (trim - ids > 5 && strncmp(ids, "type_", 5) == 0) {
        char *number_end;
        long id = strtol(ids + 5, &number_end, 10);
        if (number_end == trim && id >= 1 && id <= 43) {
          add_vc_id(&f, (int)id);
          if (id == 38) dry_thunder = true;
        }
      }
      ids = *end ? end + 1 : end;
    }
  }
  const cJSON *types = cJSON_GetObjectItemCaseSensitive(record, "preciptype");
  const cJSON *type;
  cJSON_ArrayForEach(type, types) {
    const char *s = cJSON_GetStringValue(type);
    if (equal(s, "rain")) f.flags |= WEATHER_FACT_LIQUID;
    if (equal(s, "snow")) f.flags |= WEATHER_FACT_SNOW | WEATHER_FACT_FALLING_SNOW;
    if (equal(s, "freezingrain")) f.flags |= WEATHER_FACT_FREEZING | WEATHER_FACT_LIQUID;
    if (equal(s, "ice")) f.flags |= WEATHER_FACT_ICE;
  }
  bool broad_precip_allowed = !(f.flags & (WEATHER_FACT_ICE | WEATHER_FACT_HAIL | WEATHER_FACT_FREEZING));
  bool broad_snow_allowed = broad_precip_allowed && !(f.flags & WEATHER_FACT_SNOW);
  bool broad_liquid_allowed = broad_precip_allowed && !dry_thunder;
  bool broad_thunder_allowed = !(f.flags & WEATHER_FACT_LIGHTNING) || (f.flags & WEATHER_FACT_THUNDER);
  const char *icon = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(record, "icon"));
  if (equal(icon, "clear-day") || equal(icon, "clear-night")) { if (!f.sky) f.sky = WEATHER_SKY_CLEAR; }
  else if (equal(icon, "partly-cloudy-day") || equal(icon, "partly-cloudy-night")) { if (!f.sky) f.sky = WEATHER_SKY_PARTLY; }
  else if (equal(icon, "cloudy")) { if (!f.sky) f.sky = WEATHER_SKY_CLOUDY; }
  else if (equal(icon, "fog")) f.flags |= WEATHER_FACT_FOG;
  else if (equal(icon, "wind")) f.flags |= WEATHER_FACT_WIND;
  else if (equal(icon, "rain") && broad_liquid_allowed) f.flags |= WEATHER_FACT_LIQUID;
  else if ((equal(icon, "showers-day") || equal(icon, "showers-night")) && broad_liquid_allowed) f.flags |= WEATHER_FACT_LIQUID | WEATHER_FACT_SHOWERS;
  else if (equal(icon, "snow") && broad_snow_allowed) f.flags |= WEATHER_FACT_SNOW | WEATHER_FACT_FALLING_SNOW;
  else if ((equal(icon, "snow-showers-day") || equal(icon, "snow-showers-night")) && broad_snow_allowed) {
    f.flags |= WEATHER_FACT_SNOW | WEATHER_FACT_FALLING_SNOW;
    if (!f.snow_caption) f.snow_caption = "Snow Showers";
  }
  else if (equal(icon, "thunder-rain")) {
    if (broad_thunder_allowed) f.flags |= WEATHER_FACT_THUNDER;
    if (broad_liquid_allowed) f.flags |= WEATHER_FACT_LIQUID;
  }
  else if (equal(icon, "thunder-showers-day") || equal(icon, "thunder-showers-night")) {
    if (broad_thunder_allowed) f.flags |= WEATHER_FACT_THUNDER;
    if (broad_liquid_allowed) f.flags |= WEATHER_FACT_LIQUID | WEATHER_FACT_SHOWERS;
  }
  if (isfinite(snow) && snow > 0) f.flags |= WEATHER_FACT_SNOW | WEATHER_FACT_FALLING_SNOW;
  /* Coverage is useful accompanying evidence for trend/unknown IDs. */
  const cJSON *cloud = cJSON_GetObjectItemCaseSensitive(record, "cloudcover");
  if (!f.sky && cJSON_IsNumber(cloud) && isfinite(cloud->valuedouble)) {
    f.sky = cloud->valuedouble >= 75 ? WEATHER_SKY_CLOUDY : cloud->valuedouble >= 35 ? WEATHER_SKY_PARTLY : WEATHER_SKY_CLEAR;
  }
  finish_compounds(&f);
  return f;
}

weather_picture_t weather_picture_for_facts(const weather_condition_facts_t *f)
{
  if (!f) return WEATHER_PICTURE_NONE;
  uint32_t v = f->flags;
  if (v & WEATHER_FACT_TORNADO) return WEATHER_PICTURE_TORNADO;
  if (v & WEATHER_FACT_HAIL) return WEATHER_PICTURE_HAIL;
  if (v & WEATHER_FACT_FREEZING) return WEATHER_PICTURE_FREEZING_RAIN;
  if ((v & WEATHER_FACT_THUNDER) && (v & WEATHER_FACT_FALLING_SNOW)) return WEATHER_PICTURE_THUNDERSNOW;
  if (v & (WEATHER_FACT_THUNDER | WEATHER_FACT_LIGHTNING)) return WEATHER_PICTURE_THUNDER;
  if (v & WEATHER_FACT_MIXED) return WEATHER_PICTURE_RAIN_SNOW;
  if (v & WEATHER_FACT_SNOW) return WEATHER_PICTURE_SNOW;
  if (v & WEATHER_FACT_LIQUID) return v & WEATHER_FACT_SHOWERS ? WEATHER_PICTURE_SHOWERS : WEATHER_PICTURE_RAIN;
  if (v & WEATHER_FACT_ICE) return WEATHER_PICTURE_ICE;
  if (v & WEATHER_FACT_FOG) return WEATHER_PICTURE_FOG;
  if (v & WEATHER_FACT_WIND) return WEATHER_PICTURE_WIND;
  return f->sky == WEATHER_SKY_CLEAR ? WEATHER_PICTURE_CLEAR : f->sky == WEATHER_SKY_PARTLY ? WEATHER_PICTURE_PARTLY : f->sky == WEATHER_SKY_CLOUDY ? WEATHER_PICTURE_CLOUDY : WEATHER_PICTURE_NONE;
}

const char *weather_icon_name_for_picture(weather_picture_t p, bool day)
{
  switch (p) {
    case WEATHER_PICTURE_CLEAR: return day ? "clear-day" : "clear-night";
    case WEATHER_PICTURE_PARTLY: return day ? "partlycloudy-day" : "partlycloudy-night";
    case WEATHER_PICTURE_CLOUDY: return "cloudy";
    case WEATHER_PICTURE_FOG: return "fog";
    case WEATHER_PICTURE_RAIN: return "rain";
    case WEATHER_PICTURE_SHOWERS: return day ? "showers-day" : "showers-night";
    case WEATHER_PICTURE_FREEZING_RAIN: return "freezing-rain";
    case WEATHER_PICTURE_RAIN_SNOW: return "rain-snow";
    case WEATHER_PICTURE_SNOW: return "snow";
    case WEATHER_PICTURE_HAIL: return "hail";
    case WEATHER_PICTURE_THUNDER: return "thunderstorm";
    case WEATHER_PICTURE_THUNDERSNOW: return "thundersnow";
    case WEATHER_PICTURE_ICE: return "ice";
    case WEATHER_PICTURE_WIND: return "wind";
    case WEATHER_PICTURE_TORNADO: return "tornado";
    default: return "";
  }
}

const char *weather_caption_for_facts(const weather_condition_facts_t *f, bool day)
{
  switch (weather_picture_for_facts(f)) {
    case WEATHER_PICTURE_TORNADO: return "Funnel Cloud / Tornado";
    case WEATHER_PICTURE_HAIL: return f->flags & WEATHER_FACT_THUNDER ? "Hail & Thunder" : f->hail_caption ? f->hail_caption : "Hail";
    case WEATHER_PICTURE_FREEZING_RAIN: return f->freezing_caption ? f->freezing_caption : "Freezing Rain";
    case WEATHER_PICTURE_THUNDERSNOW: return "Thundersnow";
    case WEATHER_PICTURE_THUNDER: return f->flags & WEATHER_FACT_THUNDER ? "Thunderstorm" : "Lightning";
    case WEATHER_PICTURE_RAIN_SNOW: return f->mixed_caption ? f->mixed_caption : "Rain & Snow";
    case WEATHER_PICTURE_SNOW: return f->snow_caption ? f->snow_caption : "Snow";
    case WEATHER_PICTURE_RAIN: return f->liquid_caption ? f->liquid_caption : "Rain";
    case WEATHER_PICTURE_SHOWERS: return f->liquid_caption ? f->liquid_caption : "Showers";
    case WEATHER_PICTURE_ICE: return "Ice";
    case WEATHER_PICTURE_FOG: return f->fog_caption ? f->fog_caption : "Fog";
    case WEATHER_PICTURE_WIND: return f->wind_caption ? f->wind_caption : "Wind";
    case WEATHER_PICTURE_CLEAR: return day ? "Clear" : "Clear Night";
    case WEATHER_PICTURE_PARTLY: return f->mostly_clear ? (day ? "Mostly Sunny" : "Mostly Clear") : "Partly Cloudy";
    case WEATHER_PICTURE_CLOUDY: return "Cloudy";
    default: return "Unknown";
  }
}

uint32_t weather_hourly_flags_at(const cJSON *hourly, int index)
{
  const cJSON *a = cJSON_GetObjectItemCaseSensitive(hourly, "condition_flags");
  const cJSON *v = cJSON_GetArrayItem(a, index);
  return cJSON_IsNumber(v) && isfinite(v->valuedouble) && v->valuedouble >= 0 && v->valuedouble <= UINT32_MAX ? (uint32_t)v->valuedouble : 0;
}

weather_sky_t weather_hourly_sky_at(const cJSON *hourly, int index)
{
  const cJSON *a = cJSON_GetObjectItemCaseSensitive(hourly, "sky");
  const cJSON *v = cJSON_GetArrayItem(a, index);
  return cJSON_IsNumber(v) && v->valuedouble >= WEATHER_SKY_CLEAR && v->valuedouble <= WEATHER_SKY_CLOUDY ? (weather_sky_t)v->valueint : WEATHER_SKY_UNKNOWN;
}
