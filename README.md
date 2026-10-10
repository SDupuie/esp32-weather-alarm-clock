# ESP32 Weather Alarm Clock

A round touchscreen clock for your nightstand or desk, with weather forecasts, current conditions, and messages from people at home. This project extends [Round Weather Display](https://github.com/Canterrain/round-weather-display) by Josh Hendrickson with a planned audible alarm, a battery-backed clock, and more flexible night settings.

Only the **Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C** is supported.

**Development status:** Settings persistence, draft editors, production Save handling, time-source handling and DS3231 backup-clock integration are implemented and checked natively. Normal clock screens include centered, fitted digital Night time, a shared unavailable-time screen and the approved paused-clock notice placement. The backup-clock indicator and notices are connected to the running application. Weather and House Messages remain available. Forecast labels retain provider dates, and weather downloads proceed independently of clock synchronization. Night Shift owns startup appearance, its remembered fallback and configured brightness transitions through the existing clock task. The selected brightness minimum is 20%, with a 30% Night default. Alarm playback, solar calculations and alarm screens remain pending. Sunset to Sunrise currently retains the previous appearance because its calculation is unfinished. Detailed mockup comparison found broad unfinished settings presentation, weather-state differences and small clock discrepancies; precise visual compliance remains unfinished.

Board testing confirms keyboard fit and typing, Wi-Fi connection and live weather retrieval; input-field visibility remains unresolved. The brightness change passed native/build checks and overnight observation. Controlled comparison identified unnecessary RTC access after failed initialization as a repeatable touchscreen-error trigger on this board. The small correction stops that attempt, and the 109-line manual display/touch startup workaround is removed in favor of BSP startup. Three restarted instrumented runs were clean, including an eleven-minute run through weather refresh. Final source has no diagnostic logging and passes all 15 native checks and the ESP-IDF 5.5.5 build, with 25% application partition free. All three final images were loaded and verified; a three-minute startup capture and two resumed live-state samples confirm continuing touch polling and analog clock updates. The board is left running this firmware. The user subsequently reports that everything seems to be working. Detailed physical-control checks, attached-RTC behavior, longer-term stability and the precise driver fault remain unverified. Occasional blue flashes are a separate unresolved observation. See [current development work](docs/work.md) for progress and verification limits. Native checks and compilation do not establish visual compliance or all hardware behavior.

- [Features](#features)
- [Hardware](#hardware)
- [Quick Start](#quick-start)
- [Using the Clock](#using-the-clock)
- [Household Messages](#household-messages)
- [Configuration](#configuration)
- [Development Notes](#development-notes)
- [License and Attribution](#license-and-attribution)

## Features

### Available in the current firmware

- Analog and digital clock faces, with 12-hour or 24-hour time and an optional leading zero.
- Large, centered digital time in Night Shift and a common unavailable-time screen for either clock face.
- Current temperature, today's high and low, and weather icons that change between day and night.
- A forecast for the next five days, with icons chosen to represent the daytime weather.
- A conditions screen with wind, gusts, pressure, rain or snow, and a short weather outlook.
- An edge indicator for approaching storm conditions, with quick access to the conditions screen.
- Weather from Open-Meteo, with no API key to configure.
- Time synchronized over Wi-Fi.
- DS3231 backup-clock support for startup while network synchronization proceeds, with lost-time and connection notices.
- A red Night Shift appearance with Off, Always On or Fixed Schedule preferences, remembered startup fallback, and separate Day/Night brightness with gradual transitions.
- Household messages sent from a phone or computer on the same network, with optional sharing among multiple ESP32-P4 clocks.
- Touchscreen settings for Wi-Fi, location, display, messaging and saved alarm/night preferences. Alarm operation and solar scheduling remain pending.
- Automatic startup when the board receives power.

### Planned enhancements

- One recurring alarm with selectable days, sound and volume, plus snooze and dismissal.
- Classic Beep, Gentle Chime, and Rising Tones, with a sound preview and gradually increasing alarm volume.
- Sunset to Sunrise scheduling with saved offsets.
- A redesigned interface for settings, ringing, snooze, and missed-alarm notices.

The approved design is in [docs/architecture.md](docs/architecture.md). Open [mockups/alarm-design.html](mockups/alarm-design.html) in a browser to explore the planned clock screens and House Messages page.

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

### For backup time and the planned alarm

| Item | What it is for |
| --- | --- |
| [Adafruit DS3231 Precision RTC — STEMMA QT, #5188](https://www.adafruit.com/product/5188) | Keeping time when the main board loses power. |
| [I2C wiring or an appropriate STEMMA QT cable and adapter](https://www.adafruit.com/product/4399) | Connecting the RTC to the board's existing two-wire data bus. The project's RTC wiring guide is still to be completed. |
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
2. In the empty Weather Location field, enter your location, such as `Cincinnati,OH,US` or `Paris,FR`. Tap **Save**. A successful lookup and save restarts the clock; errors retain the editor.
3. Allow time for network synchronization and the first weather update.
4. Change display or device preferences under **Device Setup**. Nested **Done** returns to the draft; **Save & Restart** applies it.

For firmware updates, build and flash again over USB. The browser installer files in [docs/index.html](docs/index.html) currently package the original weather-display baseline, without the planned alarm-clock enhancements.

## Using the Clock

Your selected analog or digital face is the home screen.

| From | Gesture or control | What happens |
| --- | --- | --- |
| Clock | Swipe left | Open Forecast, then Conditions with another left swipe. |
| Forecast or Conditions | Swipe right | Step back toward the clock. |
| Clock | Swipe right | Open Messages. |
| Messages | Swipe left | Return to the clock. |
| Clock | Swipe down | Open settings. |
| Settings editor | Tap **Cancel** | Discard that editor's changes and return to Settings. |
| Settings entry | Tap **Back** | Return to the clock after first-use location setup is complete. |
| Unread message | Tap the displayed message | Mark it read for this clock and return home after acknowledgment succeeds. |

### Weather and storm conditions

The conditions screen combines measurements with a short outlook, such as “Calm and dry” or “Thunderstorms likely by 4 PM.” Storm conditions include strong wind or gusts, thunderstorms, heavy rain or snow, and a sharp pressure drop.

During an alert, a pulsing edge indicator appears on the clock. It is amber during the day and red in Night Shift. Swiping left opens **Conditions first**, then Forecast.

Weather normally refreshes every ten minutes. If an update fails, the clock keeps the last available weather while retrying. That retained weather may be out of date.

### Night Shift

Night Shift supplies Day or Night for Off, Always On and the saved Fixed Schedule, with separate configured brightness and gradual transitions, including startup. Both brightness controls use the approved minimum; a saved value below it is raised for the running display without rewriting storage. Existing choices within the allowed range are retained. When a scheduled mode cannot be calculated at startup, it uses the last saved appearance, or Day if none is usable. Solar calculations remain pending; that mode currently uses the fallback and retains the selected appearance. See the [Night Shift rules](docs/architecture.md#night-shift) for the range, defaults and accepted save-failure behavior. The board has visibly responded to brightness changes; low-light usability and fade smoothness remain under test.

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

1. Put all clocks on the same local network.
2. Give each a recognizable room name, such as `Kitchen` or `Bedroom`.
3. Give each a **distinct device ID**, such as `kitchen-clock` or `bedroom-clock`.
4. Select **Shared** messaging and tap **Save & Restart** on each clock.

Shared mode rejects the default `clock-esp32-p4` ID. The clocks find each other automatically and choose one to coordinate messages. You can open any participating clock's messages page and choose the target there. The network must allow local broadcast discovery; isolation between Wi-Fi clients can prevent sharing.

**Message retention:** Current messages are kept in memory. Do not rely on them surviving a restart or loss of power; there is no persistent message archive.

## Configuration

Swipe down from the clock to open settings. Saved preferences stay on the device across restarts; there is no user-editable settings file to maintain.

| Setting | What it controls |
| --- | --- |
| Wi-Fi | Network and password; tap **Wi-Fi** to scan or enter a network manually. |
| Weather location | The place used for weather and the clock's timezone. Enter `City, State`, optionally followed by a country code, such as `Cincinnati,OH,US`. For a city and country without a state, use `Paris,FR`. |
| Room name | The friendly name shown when choosing message targets. |
| Device ID | The identity used by shared messaging; choose a distinct value for every shared clock. |
| House Messages | **Single** for this clock or **Shared** for several clocks. |
| Clock face | **Analog** or **Digital**. |
| Time format | **12-hour** or **24-hour**. |
| Leading zero | Whether 12-hour time displays `07:00 AM` or `7:00 AM`. |
| Units | **Imperial** or **Metric** weather measurements. |
| Night Shift | Mode, fixed schedule and Day/Night brightness; solar offsets are saved, but solar calculations remain pending. |
| Alarm | Saved time, repeat days, sound, volume, snooze and enable preference; ringing and preview are pending. |

Device Setup and Wi-Fi share a draft for **Save & Restart**. Weather Location saves independently and restarts after success. Alarm **Save** and the Settings-entry alarm toggle apply settings without restarting. Nested **Done** keeps edits in the parent draft; **Cancel** discards that editor's edits. See the [architecture](docs/architecture.md#editing-and-saving-user-settings) for save rules and accepted partial-save behavior.

Settings screens currently use native rollers and the LVGL keyboard with the original three layouts and the added colon. Tapping a text field opens the centered keyboard; its Done or hide key reveals the fields again. Tapping the same field reopens it after hiding it. Exact approved control styling and the broader replacement interface remain pending; the mockup remains the visual authority.

## Development Notes

The firmware is written in C using **ESP-IDF 5.5.5**. **LVGL** draws the touchscreen interface. The firmware project and shell helpers live in `targets/esp32-p4/`. One application component groups service owners under `main/`, UI files under `main/ui/`, and artwork/fonts under `main/assets/`; native tests live under `tests/`.

`shared/` retains the weather icon sources and weather/location test data used by the ESP32 tools and native C checks.

Read [AGENTS.md](AGENTS.md) before changing the project. The following documents cover the detailed design and development workflow:

| Document | What it covers |
| --- | --- |
| [docs/architecture.md](docs/architecture.md) | Approved behavior, defaults, ownership, source structure, and links to the visual design. |
| [docs/work.md](docs/work.md) | Current development tasks, completed slices and the next slice. |
| [targets/esp32-p4/README.md](targets/esp32-p4/README.md) | Environment preparation, build, flash, monitor, and browser-firmware preparation. |
| [targets/esp32-p4/tests/README.md](targets/esp32-p4/tests/README.md) | Existing checks, how to run them, and verification limits. |

The source structure in the architecture document is a plan; listed enhancement files are created only when needed. The mockup is a visual reference, not firmware or a hardware emulator.

The [development-status summary](#esp32-weather-alarm-clock) and [current work record](docs/work.md) distinguish native/build results, observed board behavior and remaining hardware verification.

## License and attribution

This project is licensed under the [Creative Commons Attribution-NonCommercial 4.0 International License](https://creativecommons.org/licenses/by-nc/4.0/).

Original project © 2026 Josh Hendrickson. [Author](https://anoraker.com). The original project credits [MagicMirror](https://github.com/MagicMirrorOrg/MagicMirror) as inspiration.
