# Changelog

## Application changes from Round Weather Display

- Added an audible alarm with one-time and weekday repeats, sound previews,
  gradual ringing volume, Snooze, Slide to Dismiss and restart recovery.
- Added Sound Machine with nine looping recordings, three generated noises,
  independent volume and sleep timers.
- Added optional DS3231 backup time and locally calculated Sunset to Sunrise
  Night Shift, with separate Day/Night brightness and remembered appearance.
- Added Visual Crossing alongside Open-Meteo, expanded weather-picture mappings,
  dated five-day forecasts and concise waiting/stale-weather notices.
- Added full-screen U.S. Radar from RainViewer and USGS, covering 75 miles
  with a Light/Moderate/Heavy key; it replaces Conditions in device navigation.
- Reworked touchscreen settings to the approved design, including visible
  keyboard inputs, live Display Save and an immediate Alarm On/Off switch.
- Simplified application ownership and removed superseded screen experiments
  and routine clock-footer messages. Improved audio buffering, memory placement,
  weather scheduling and drawing work while preserving approved behavior.

See the [user guide](README.md) for current features and source credits. The
[work record](docs/work.md) owns implementation details, test evidence and
remaining work; this summary does not establish hardware verification.

## Inherited change note

Fix: prevent weekly Sunday freeze by adding request timeouts and serving last-known-good weather data.
