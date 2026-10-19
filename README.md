# ESP32 Weather Alarm Clock

A round touchscreen clock for your nightstand or desk, with weather forecasts, current conditions, and messages from people at home. This project extends [Round Weather Display](https://github.com/Canterrain/round-weather-display) by Josh Hendrickson with an audible alarm, optional battery-backed timekeeping, and more flexible night settings.

Only the **Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C** is supported.

- [Features](#features)
- [Hardware](#hardware)
- [Quick Start](#quick-start)
- [Using the Clock](#using-the-clock)
- [Household Messages](#household-messages)
- [Configuration](#configuration)
- [Documentation](#documentation)
- [License and Attribution](#license-and-attribution)

## Features

- Analog and digital clock faces, with 12-hour or 24-hour time and an optional leading zero in 12-hour mode.
- Large, centered digital time in Night Shift and a common unavailable-time screen for either clock face.
- Current temperature, today's high and low, and weather icons that change between day and night.
- A forecast for the next five days, with icons chosen to represent the daytime weather.
- A current radar map for U.S. weather locations, centered on the saved location with a full-screen map covering a 75-mile diameter.
- Official National Weather Service watches and warnings, with on-clock headlines, warning/watch counts, and an alert list with event details.
- Weather from Open-Meteo without a key, or Visual Crossing with your API key.
- Time synchronized over Wi-Fi.
- DS3231 backup-clock support for startup while network synchronization proceeds, with lost-time and connection notices.
- A red Night Shift appearance with Off, Always On, Fixed Schedule or locally calculated Sunset to Sunrise preferences, remembered startup fallback, and separate Day/Night brightness with gradual transitions.
- Household messages sent from a phone or computer on the same network, with optional sharing among multiple ESP32-P4 clocks.
- Touchscreen settings for Wi-Fi, location, display, messaging and saved alarm/night preferences.
- One alarm with one-time or selected repeat days, Snooze, Slide to Dismiss while ringing, missed-alarm notices and restart recovery.
- Classic Beep, Classic Bell, and Rising Tones, with a five-second sound preview and gradually increasing alarm volume.
- Sound Machine with nine repeating recordings, White/Pink/Brown Noise, independent volume and a sleep timer.
- Automatic startup when the board receives power.

The approved design is in [docs/architecture.md](docs/architecture.md). Open [mockups/alarm-design.html](mockups/alarm-design.html) in a browser to explore the clock screens and House Messages page.

## Hardware

### For the current weather clock

| Item | What it is for |
| --- | --- |
| [Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C](https://www.amazon.com/dp/B0F9YP4DCS) | The board with its 3.4-inch, 800 × 800 round touchscreen and included speaker. |
| [USB-C **data** cable (USB-C to USB-C)](https://www.amazon.com/dp/B0CN645NJ3) | Connecting the board to a computer for installation and updates; the linked cable supports 480 Mbps data transfer. A charge-only cable cannot transfer firmware. |
| USB power source | Powering the clock during everyday use. |
| Computer with Git and ESP-IDF 5.5.5 | Downloading, building, and installing the firmware. |
| Wi-Fi network with internet access | Weather updates and network time. |
| Phone or computer with a browser | Sending House Messages over the same local network. |

Waveshare's [hardware documentation](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-XC#hardware-description) identifies the USB power/flashing ports and board connectors.

### For backup time

| Item | What it is for |
| --- | --- |
| [Adafruit DS3231 Precision RTC — STEMMA QT, #5188](https://www.adafruit.com/product/5188) | Keeping time when the main board loses power. |
| [I2C wiring or an appropriate STEMMA QT cable and adapter](https://www.adafruit.com/product/4399) | Connecting the RTC to the board's existing two-wire data bus. |
| CR1220 coin cell | Battery backup for the Adafruit RTC module; the module does not include the battery. |

The CR1220 goes in the **Adafruit RTC module**, not the Waveshare board's RTC battery connector. See the [Adafruit module documentation](https://www.adafruit.com/product/5188) and [Waveshare connector specifications](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-XC#hardware-description).

The RTC battery keeps time; it does not power the display or sound an alarm while the main board is unplugged.

## Quick Start

This workflow builds from source. The shell examples use macOS/Linux; the detailed environment and build instructions are in [targets/esp32-p4/README.md](targets/esp32-p4/README.md).

### 1. Get the project

Install Git, then clone this repository:

```bash
git clone https://github.com/SDupuie/esp32-weather-alarm-clock.git
cd esp32-weather-alarm-clock
```

### 2. Install and activate ESP-IDF

Install **ESP-IDF 5.5.5** and the **ESP32-P4** toolchain outside the repository. Use Espressif's [ESP-IDF Installation Manager](https://docs.espressif.com/projects/idf-im-ui/en/latest/installation_methods.html) or its [ESP32-P4 installation guide](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32p4/get-started/index.html).

Activate the installation using its supplied activation script. For the installation layout used by this project, run from the repository root:

```bash
source targets/esp32-p4/scripts/activate-idf.sh
idf.py --version
```

The helper expects `~/.espressif/tools/activate_idf_v5.5.5.sh`. If your installation uses a different location, source the installer's activation script instead. Confirm that the reported ESP-IDF version is 5.5.5.

VS Code is optional. If you use it, install Espressif's [ESP-IDF extension](https://github.com/espressif/vscode-esp-idf-extension) and open `targets/esp32-p4` as the firmware project folder.

### 3. Build

From the repository root, in the activated environment:

```bash
targets/esp32-p4/scripts/build.sh
```

You can build without connecting a board.

### 4. Install on the board

Connect the board with the USB-C data cable. Replace the example port below with your board's serial port:

```bash
targets/esp32-p4/scripts/flash.sh /dev/tty.usbmodemXXXX
targets/esp32-p4/scripts/monitor.sh /dev/tty.usbmodemXXXX
```

The example port is for macOS. See Espressif's [serial connection instructions](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32p4/get-started/establish-serial-connection.html) to identify your board's port. The [monitor instructions](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32p4/get-started/linux-macos-setup.html#monitor-the-output) explain the startup log; press **Ctrl+]** to exit it.

### 5. Finish setup on the touchscreen

1. Open **Wi-Fi** under Device Setup, select your network and enter its password. Tap **Save & Restart**. Use **Manual Network** if needed.
2. Open **Device Setup → Weather → Weather Location**, enter your location, such as `Cincinnati,OH,US` or `Paris,FR`, and tap **Save**. A successful lookup and save restarts the clock; errors retain the editor.
3. Allow time for network synchronization and the first weather update.
4. Open **Settings → Display Settings** to change the clock face, time format or brightness, then tap **Save** to apply without restarting. Other device preferences are under **Device Setup**; nested **Done** keeps your edits until **Save & Restart** applies them.

For firmware updates, build and flash again over USB. The browser installer in
[docs/index.html](docs/index.html) uses four prepared images, including the Sound
Machine recordings. See the [target guide](targets/esp32-p4/README.md#prepare-browser-installation-files)
to rebuild those installation files before distributing updated firmware.

## Using the Clock

Your selected analog or digital face is the home screen.

| From | Gesture or control | What happens |
| --- | --- | --- |
| Clock | Swipe left | Open Forecast, then Radar with another left swipe for supported U.S. locations. |
| Forecast or Radar | Swipe right | Step back toward the clock. |
| Clock | Swipe right | Open Messages. |
| Messages | Swipe left | Return to the clock. |
| Clock | Swipe down | Open settings. |
| Clock | Tap the speaker icon | Open Sound Machine; Back returns to the clock. |
| Clock | Tap the alert triangle | Open the active-alert list. Tap an on-clock alert headline to dismiss it from both clock faces. |
| Clock | Tap the next-alarm or **Alarm off** notice | Open Alarm Settings. |
| Clock | Tap **Missed Alarm** | Dismiss the missed-alarm notice. |
| Clock | Tap the snoozed-alarm notice | Open the snooze countdown screen. |
| Clock | Tap the backup-clock warning triangle, when shown | Read the backup-clock notice and dismiss it after checking the problem. |
| Settings editor | Tap **Cancel** | Discard that editor's unsaved changes; Weather returns to Device Setup, while Display, Device Setup and Alarm return to Settings. |
| Settings entry | Tap **Back** | Return to the clock after first-use location setup is complete. |
| Unread message | Tap the displayed message | Mark it read for this clock and return home after acknowledgment succeeds. |

### Weather alerts and Radar

Radar shows the latest [RainViewer](https://www.rainviewer.com/) frame, with
smoothed precipitation edges, over a
[USGS National Map](https://www.usgs.gov/programs/national-geospatial-program/national-map)
background, with the map's printed town names, a marker at your saved weather
location and the frame's local date/time. The key uses equal-width green, yellow
and red sections labeled **Light**, **Moderate** and **Heavy**. These describe
radar echo strength, not NWS alert severity. Radar checks for a newer frame
every five minutes. An unsuccessful update
keeps the previous image and marks it “Old”; before any image arrives it shows
“Radar unavailable.” This first version supports locations resolved in the
United States. Radar is hidden for other locations; Forecast remains available.
Tap **Play** above the key to play the six latest ten-minute frames once, aiming
for 1.5 seconds per image. Playback stops on the newest image and shows Play
again. The usual five-minute checks keep that still image current.
A busy update can hold an image longer. Tap **Pause** to
hold the displayed image; its timestamp says
**Paused**. Play resumes with the next image, or finishes if already at the
newest frame. Leaving Radar stops playback and
returns to the latest still image. The control becomes available after history
loads; fewer images may be available after a failed download. Six samples cover
the latest hour, with fifty minutes between the oldest and newest timestamps.
The fixed map has no zoom or panning controls.

Official watches and weather warnings come from the
[National Weather Service Alerts API](https://api.weather.gov/alerts), using the
saved weather location and coordinates. This works independently of the
selected forecast provider. The clock keeps the weather-alert triangle visible;
red warning and yellow watch badges show active counts. When an eligible alert
headline is displayed, tap its text to dismiss the headline on both clock faces.
The active event remains in the alert list and counts.

Tap the triangle to open **Alerts**. The list includes all active watches and
warnings in NWS priority order, including alerts whose clock headline was
dismissed. Select a row to read its affected area, issue/end times, description
and protective instructions. The list reports when current alerts could not be
updated or saved, and identifies locations where NWS alerts are unavailable.

From the clock, swiping left always opens **Forecast** first and then **Radar**
for supported U.S. locations. An alert does not change this order. Forecast is
the only weather page for other locations.

Weather normally refreshes every ten minutes. Until the first successful update,
the clock and Forecast show **Waiting for weather…** with no invented readings.
If a later update fails, the clock keeps the last available weather while
retrying. The clock faces show its age, such as **Weather updated 2h ago**, or
**Weather data stale** when the age cannot be trusted. Forecast uses **Weather
data stale** above its main day heading. Clock-face weather details and their
notices are hidden in Night appearance.

In **Device Setup → Weather**, choose weather data by
[Open-Meteo](https://open-meteo.com/) without a key or
[Visual Crossing Weather](https://www.visualcrossing.com/) with your API key.
Location searches use Open-Meteo for either weather provider. The [configuration guide](#configuration)
explains activation checks, key retention and clearing.

The weather pages reached by swiping are Forecast and, for U.S. locations,
Radar.

### Clock and backup-clock notices

| Notice | What it means |
| --- | --- |
| **Waiting for sync** | The backup clock supplied usable time, but internet time has not yet synchronized after startup. The clock and alarms can still run. |
| **Time unavailable** | No usable time is available yet. The clock waits before triggering alarms. |
| **Clock paused** | The displayed minute has stopped advancing while the display is still being updated. |
| **Backup Clock Lost Time / Check Battery** | The RTC reported that its time could not be trusted. Check its coin cell. |
| **Backup Clock Unavailable / Check Connection** | The clock could not access the RTC. Check its connection. |

The triangle opens either backup-clock notice. Dismiss acknowledges the notice;
it does not repair the hardware. Internet synchronization can restore time
without clearing an undismissed notice. The RTC is checked again on each startup.

**Settings storage unavailable** is a separate startup failure. The clock stops
startup when it cannot load settings, rather than automatically erasing them.

### Night Shift

Night Shift supplies Day or Night for Off, Always On, the saved Fixed Schedule and locally calculated Sunset to Sunrise, with separate configured brightness and gradual transitions, including startup. Solar scheduling uses the saved weather location and usable clock time, so weather downloads are not needed for its daily calculation. The offsets page shows tonight's start and tomorrow's end, including draft offsets; editing those values does not change the running schedule until Save. Both brightness controls use the approved minimum; a saved value below it is raised for the running display without rewriting storage. Existing choices within the allowed range are retained. When a scheduled mode cannot be calculated at startup, it uses the last saved appearance, or Day if none is usable; unavailable inputs while running retain the current appearance. See the [Night Shift rules](docs/architecture.md#night-shift) for the range, defaults and accepted save-failure behavior.

**Low brightness:** The 20% setting can make the screen too dim to use its menus. Check that the display remains readable before saving a lower brightness.

## Household Messages

The clock serves a **House Messages** web page. Connect your phone or computer to the same local network and open:

```text
http://<clock-ip>/messages
```

Replace `<clock-ip>` with the address shown for the clock in your router's connected-device list. Opening `http://<clock-ip>/` also opens the messages page.

### Send and manage messages

1. Enter a message and, optionally, a sender name.
2. Choose a target clock or **All clocks**.
3. Select **Normal** or **Important** priority.
4. Optionally set an expiration date and time.
5. Tap **Send Message**.

An edge indicator shows unread messages; Important messages make it pulse. Swipe right from the clock to read them. Important messages appear before Normal messages, with newer messages first within each priority.

Tapping a message on the clock marks it read **for that clock**. The browser's **Deactivate** action makes it inactive for everyone. Expired messages stop appearing.

While dismissal is pending, the clock remains responsive and shows “Dismissing...”. It returns home after acknowledgment succeeds. Failure keeps the message visible with a retry prompt. Leaving the message screen prevents a later result from taking you back home.

The browser's **Active Messages** list shows all active, unexpired messages, including those sent to an individual clock. Messages remain in this list after a clock marks them read; use **Deactivate** to remove them for everyone.

### Share messages among clocks

For one clock, leave messaging in **Single** mode. For several ESP32-P4 clocks:

Open **Settings → Device Setup → Messaging**, then:

1. Put all clocks on the same local network.
2. Give each a recognizable room name, such as `Kitchen` or `Bedroom`.
3. Give each a **distinct device ID**, such as `kitchen-clock` or `bedroom-clock`.
4. Select **Shared**, tap **Done**, then **Save & Restart** on Device Setup for each clock.

Shared mode rejects the default `clock-esp32-p4` ID. The clocks find each other automatically and choose one to coordinate messages. You can open any participating clock's messages page and choose the target there. The network must allow local broadcast discovery; isolation between Wi-Fi clients can prevent sharing.

**Message retention:** Current messages are kept in memory. Do not rely on them surviving a restart or loss of power; there is no persistent message archive.

## Configuration

Swipe down from the clock to open settings. Saved preferences stay on the device across restarts; there is no user-editable settings file to maintain.

Room Name, Device ID and House Messages are under **Device Setup → Messaging**. Tap either name/ID row to edit its text. **Done** returns to Device Setup with those edits pending.

| Setting | What it controls |
| --- | --- |
| Wi-Fi | Network and password; tap **Wi-Fi** to scan or enter a network manually. |
| Weather location | The place used for weather and the clock's timezone. Enter `City, State`, optionally followed by a country code, such as `Cincinnati,OH,US`. For a city and country without a state, use `Paris,FR`. |
| Provider / API key | Choose Open-Meteo or Visual Crossing in **Device Setup → Weather → Provider**. Visual Crossing's API Key editor is nested under Provider; a key is checked when first selected or changed. |
| Alerts | In **Device Setup → Weather → Alerts**, choose which watches and warnings can sound, alert-update sounds and whether those sounds can play during Night Shift. Visual alerts remain enabled when sounds are silent. |
| Room name | The friendly name shown when choosing message targets. |
| Device ID | The identity used by shared messaging; choose a distinct value for every shared clock. |
| House Messages | **Single** for this clock or **Shared** for several clocks. |
| Clock face | **Analog** or **Digital**. |
| Time format | **12-hour** or **24-hour**. |
| Leading zero | Whether 12-hour time displays `07:00 AM` or `7:00 AM`. In 24-hour mode it shows On and cannot be changed; switching back restores the previous 12-hour choice. |
| Units | **Imperial** or **Metric** weather measurements. |
| Brightness | Day/Night levels under **Settings → Display Settings → Brightness**. |
| Night Shift | Mode, fixed schedule, and solar offsets with calculated start/end times. |
| Alarm | Time, Repeat None for one-time or selected weekdays, sound, volume, snooze and On/Off. The editor previews the next alarm for unsaved time/day edits. |
| Sound Machine | Sound, independent volume, sleep timer and Play/Stop. |

Display Settings has its own **Cancel** and **Save**. Save applies the clock face, time format, leading zero and Day/Night brightness without restarting, then returns to Settings. Cancel or Back discards those edits.

Radar frame timestamps also use the saved time format, including while showing
an image retained after a failed update.

Device Setup and Wi-Fi share a draft for **Save & Restart**. The **Weather** entry opens the Weather editor for location, provider, units and API key, which save independently and restart after success. Its Back or Cancel returns to Device Setup with any other device edits retained; Weather Save applies only the weather choices. Alarm **Save** applies settings without restarting. The **Alarm On/Off** switch in Alarm Settings saves immediately; **Cancel** does not undo a successful switch change. Other alarm edits still require **Save**. Nested **Done** keeps edits in the parent draft; **Cancel** discards unsaved edits. See the [architecture](docs/architecture.md#editing-and-saving-user-settings) for save rules and accepted partial-save behavior.

Visual Crossing's API key stays masked. **Configured** means a key is saved;
**Entered (not saved)** means you changed it in the editor. Save checks a newly
selected Visual Crossing provider or changed key before applying it. If the
check fails, the editor stays open for retry without restarting. Choosing
Open-Meteo keeps the saved key for later reuse. To remove it, open API Key while
Visual Crossing is selected, tap **Clear**, close the keyboard, choose
Open-Meteo and Save. Cancel discards unsaved key changes.

Alerts use two separate sound catalogs: 23 NWS watches and 32 weather warnings.
Watch Sounds offers **Off**, **All watches** or individual watches; Warning
Sounds offers **Off**, **All warnings** or individual warnings. Their defaults
are Off for watches and All warnings for warnings. **Alert Update Sounds**
offers Off, Increased danger only or All updates and defaults to Off.
**Night Alert Sounds** defaults to Off. These choices use the alarm volume;
there is no separate alert-volume control. The Weather screen's Alerts summary
describes the selected watch/warning sounds; **Silent** means sounds are off,
not that visual alerts are disabled. See the [approved weather-alert behavior](docs/architecture.md#nws-watches-and-warnings) for the full rules.

If Visual Crossing confirms that the key's daily allowance is exhausted, the
clock keeps its last weather and waits for the provider's retry time, or checks
once an hour when none is supplied. You can choose another provider yourself.

Tap a text field to open the keyboard. The active input stays visible above it; Done or Close returns to the menu with your unsaved text. Passwords are masked.

Alarm Time and Fixed Schedule minute arrows move in five-minute steps and change the hour when minutes wrap. Opening an editor leaves the saved time unchanged. Volume and brightness sliders have a thin bar and a large circular handle.

Preview Sound plays the selected draft sound at the draft volume without saving either choice. Stop Preview, leaving the sound picker or a ringing alarm stops the preview. Sunrise/Sunset shows calculated event times when the clock has usable time and a saved location.

While the alarm rings, tap **SNOOZE**, or drag the left handle of **SLIDE TO
DISMISS** to the right end and release. Tapping the track or releasing a short
drag keeps it ringing. Dismissal shows **DISMISSED** briefly before returning
to the clock. The snoozed screen keeps its tap **DISMISS** button.

For a one-time alarm, choose **Repeat → None**, set the time and Save. If Alarm
is Off, turn it On after saving those choices. It rings at the next occurrence
of that time, today or tomorrow. Snooze works normally;
Dismiss turns the alarm Off. Turn it On again to reuse the time for another
one-time alarm. Restart recovery keeps the original scheduled date.

Repeat also offers **Every Day**, **Weekdays** and **Custom Days**. Custom Days
lets you select individual weekdays. Use Done or Back to return through Repeat
to Alarm, then Save to apply the selection. Cancel on Alarm discards it.

Turning Alarm Off stops ringing and cancels a pending snooze. Turning it On
after today's alarm time arms the next scheduled alarm. After a restart, an
unhandled alarm no more than 30 minutes overdue rings when usable time returns;
an older ordinary occurrence produces **Missed Alarm**. A saved snooze resumes
its countdown, or rings if its deadline is no more than 30 minutes overdue;
an older snooze is discarded. The backup battery does not sound the alarm while
main power is absent.

### Sound Machine

Open **Settings → Sound Machine** or tap the speaker on either clock face.
Choose from **Rain, Thunder, Ocean, Wind, Forest, Campfire, Stream, Night,
Singing Bowl, White Noise, Pink Noise** and **Brown Noise**. The first nine are
looping recordings; the three noises are generated continuously. Sleep Timer
offers **Continuous**, **15**, **30**, **60** or **120 minutes**.

Choose a sound, volume and sleep timer, then tap **Play**. Sound, volume and timer
changes save and apply immediately; **Back** and **Done** keep playback running.
Changing the sound keeps the remaining time; changing the timer starts its full
duration. Timed playback fades during its final 30 seconds. **Stop**, a ringing
alarm or an alarm-sound preview ends playback. Press Play to start again.
After a restart, your choices remain saved and playback is Stopped.
See the [playback rules](docs/architecture.md#sound-machine-playback) for details.

## Documentation

See the [firmware setup and update guide](targets/esp32-p4/README.md) for detailed installation instructions and the [approved design](docs/architecture.md) for product behavior. The [weather mapping reference](docs/weather.md) shows our icons, both providers' mappings and source limitations. Development work and verification records are kept in [docs/work.md](docs/work.md).

## License and attribution

This project is licensed under the [Creative Commons Attribution-NonCommercial 4.0 International License](https://creativecommons.org/licenses/by-nc/4.0/).

Original project © 2026 Josh Hendrickson. [Author](https://anoraker.com). The original project credits [MagicMirror](https://github.com/MagicMirrorOrg/MagicMirror) as inspiration.

### Weather, location and map sources

- **Weather data by [Open-Meteo](https://open-meteo.com/)** — the default forecast
  and current-weather source. API data is provided under
  [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/); access to its free
  service follows the [Open-Meteo terms](https://open-meteo.com/en/terms).
- **Weather data provided by [Visual Crossing Weather](https://www.visualcrossing.com/)**
  — the optional keyed forecast and current-weather source, under its
  [weather data license](https://www.visualcrossing.com/weather-service-terms/)
  and [attribution guidance](https://www.visualcrossing.com/resources/documentation/weather-data/how-to-properly-cite-or-attribute-visual-crossing-weather-data/).
- **Official watches and weather warnings by the [National Weather Service](https://api.weather.gov/alerts)** — active alerts for the clock's saved weather location.
- **Location data by [GeoNames](https://www.geonames.org/), via
  [Open-Meteo geocoding](https://open-meteo.com/en/docs/geocoding-api#attribution)**
  — place names, coordinates and timezones used by location setup, including
  when Visual Crossing supplies the weather. GeoNames data is
  [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
- **Radar data by [RainViewer](https://www.rainviewer.com/)** — latest radar
  overlays, recolored for the clock's green/yellow/red key. See the
  [RainViewer API terms](https://www.rainviewer.com/api.html).
- **Map services and data available from U.S. Geological Survey, National
  Geospatial Program.** The Radar background uses USGSTopo from
  [The National Map](https://www.usgs.gov/programs/national-geospatial-program/national-map).
  These map services and data are
  [public domain](https://www.usgs.gov/faqs/what-are-terms-uselicensing-map-services-and-data-national-map);
  the clock adjusts their brightness for its display.

The clock converts provider units, selects representative forecast pictures and
creates its own weather captions. These changes do not imply endorsement by the
data providers. External data, libraries and assets retain their own licenses;
the project's license does not replace them.

### Sound sources

Sound Machine recordings and the White, Pink and Brown Noise calculations were
adapted from [soundmachine.app](https://soundmachine.app/). The clock's recordings
have prepared loop crossfades and playback gain adjustments; generated noises
use the source equations with the clock's output gain.
