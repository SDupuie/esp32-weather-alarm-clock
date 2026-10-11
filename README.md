# ESP32 Weather Alarm Clock

A round touchscreen clock for your nightstand or desk, with weather forecasts, current conditions, and messages from people at home. This project extends [Round Weather Display](https://github.com/Canterrain/round-weather-display) by Josh Hendrickson with a planned audible alarm, a battery-backed clock, and more flexible night settings.

Only the **Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C** is supported.

**Development status:** Settings persistence, draft editors, production Save handling, time-source handling and DS3231 integration are implemented and checked natively. Weather and House Messages remain available; forecast labels use provider dates and weather downloads proceed independently of clock synchronization. Normal clock screens include centered digital Night time, shared unavailable time, the paused-clock notice and the “Waiting for sync” notice while backup-clock time awaits network synchronization. The approved clock positioning/spacing, weather-waiting displays, Forecast/Conditions warning presentation and divider styling are implemented. The four daily Forecast columns are now centered in the source and mockup; that follow-up passes native/build checks and is loaded, with verified images and running identity. The user confirms the four columns look centered and unclipped. Night Shift handles remembered startup appearance and brightness transitions, with a 20% minimum and 30% Night default. Settings menus have been reworked against the approved mockup.

The loaded screen-update firmware passed all 17 native checks and the ESP-IDF 5.5.5 build, with 24% application partition free; all three written images were verified and its running identity matches the build. A 35-second startup capture confirms display/touch initialization and live-weather retrieval without a panic or touch-read failure. The earlier settings-menu basic board-test pass is complete: menu controls, text entry, nearby/manual Wi-Fi pages, time stepping and hour wrapping, selected marks and sliders work in the prompted checks. The checked Day/Night pages fit and remain readable. Device Setup Save & Restart returns to the clock with working Settings. At the end of that menu-test checkpoint, the temporary Night test was finished and Sunset to Sunrise mode and Day appearance were restored; later saved choices are recorded in the current work log.

The approved Digital Night size follow-up is implemented in the new build, with the mockup updated. All 17 native checks pass, including every hour/minute in all three time formats, and the firmware build passes with 24% application partition free. This follow-up is now loaded: all three written images passed verification, the running identity matches, and a 35-second capture shows display/touch initialization and live weather without a panic or touch-read failure. The user confirms both selected Night formats fit and remain readable; exact mockup appearance remains a separate check. See [current development work](docs/work.md) for its scope and verification limits.

The latest minimum-brightness board check failed: at 20%, the user could not see the screen or recover through its menus. A temporary in-memory adjustment brought the backlight control to 50%; the user confirms visible output and responsive Settings. A later read-only storage check found Day 30%, Night 26% and Always On Night; those saved choices were preserved during the latest firmware load. Selecting a revised minimum remains pending. The recovery did not change stored preferences or the firmware's current 20% limit; physical visibility at the later saved Night value has not been confirmed. See the [recovery record](.reviews/main/brightness-recovery-2026-10-11/summary.md).

Audio preview, alarm operation and solar calculations remain unfinished; menus show their disabled/unavailable state. Detailed physical mockup comparison, exceptional states and broader persistence/hardware checks remain unverified. The saved-weather caption uses the actual timezone identifier through the reference fallback. The approved screen walkthrough is implemented and checked natively; the user confirms Digital Day label fit/readability and Digital/Conditions divider visibility/readability. Further physical comparison remains pending. See [current development work](docs/work.md) for the completed slices and verification limits.

Earlier controlled board tests identified unnecessary RTC access after failed initialization as a repeatable touchscreen-error trigger. The correction restores standard board display/touch startup and removes the manual timeout workaround; restarted runs and a weather-refresh observation passed. Detailed evidence is linked from [current development work](docs/work.md). Attached-RTC operation, longer-term stability and the precise driver fault remain unverified. Occasional blue flashes remain a separate unresolved observation. Native checks, compilation and this basic board pass do not establish exact visual compliance or every hardware/failure state.

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
- Ringing, snooze and missed-alarm screens.

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

The current source uses the mockup’s four LVGL keyboard pages. Tapping a text field temporarily hides inactive controls and keeps the active input above the keyboard. Done or Close restores the menu and its unsaved values. Native checks cover typing, password masking and the whole composition. The user confirms Weather Location input visibility, keyboard fit and typing, plus selected-network password entry on the board. The user also confirms visible input and normal typing for Room Name and Device ID, plus fit and operation of all four keyboard pages in Room Name. A separate comparison in both appearances remains pending. Time, repeat, offset, sound and brightness controls also follow the measured mockup. Sliders use a thin bar and a larger circular handle while retaining a generous touch area. Alarm and Fixed Schedule minute arrows use five-minute steps and change the hour when minutes wrap; existing saved times are retained until edited. See the [time-control decision](docs/architecture.md#ui-and-visual-reference). Preview Sound is disabled until playback is implemented; Sunrise/Sunset shows unavailable event times until local calculation is implemented.

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
