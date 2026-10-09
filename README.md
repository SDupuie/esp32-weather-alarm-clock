# ESP32 Weather Alarm Clock

A round touchscreen clock for your nightstand or desk, with weather forecasts, current conditions, and messages from people at home. This project extends [Round Weather Display](https://github.com/Canterrain/round-weather-display) by Josh Hendrickson with a planned audible alarm, a battery-backed clock, and more flexible night settings.

Only the **Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C** is supported.

**Development status:** The current firmware provides the original weather clock and House Messages. The alarm, DS3231 backup clock, expanded Night Shift controls, and redesigned interface are approved but not implemented yet. The setup and usage instructions below describe the current firmware.

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
- Current temperature, today's high and low, and weather icons that change between day and night.
- A forecast for the next five days, with icons chosen to represent the daytime weather.
- A conditions screen with wind, gusts, pressure, rain or snow, and a short weather outlook.
- An edge indicator for approaching storm conditions, with quick access to the conditions screen.
- Weather from Open-Meteo, with no API key to configure.
- Time synchronized over Wi-Fi.
- An optional red Night Shift appearance from 10 PM to 6 AM.
- Household messages sent from a phone or computer on the same network, with optional sharing among multiple ESP32-P4 clocks.
- Touchscreen settings for Wi-Fi, location, clock appearance, units, and messaging.
- Automatic startup when the board receives power.

### Planned enhancements

- One recurring alarm with selectable days, sound and volume, plus snooze and dismissal.
- Classic Beep, Gentle Chime, and Rising Tones, with a sound preview and gradually increasing alarm volume.
- A DS3231 real-time clock to retain time while the board is unplugged and provide time before network synchronization.
- Expanded Night Shift choices: Off, Always On, Fixed Schedule, and Sunset to Sunrise, with separate day/night brightness and gradual transitions.
- A redesigned interface for settings, ringing, snooze, and clock notices.

The approved design is in [docs/wac/architecture.md](docs/wac/architecture.md). Open [mockups/alarm-design.html](mockups/alarm-design.html) in a browser to explore the planned clock screens and House Messages page.

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

### For the planned alarm and backup clock

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

1. Select your Wi-Fi network, enter its password, and tap **Save & Restart**. Use **Manual Network** if needed.
2. In the location/settings screen, replace the prefilled location with your own, such as `Cincinnati,OH,US` or `Paris,,FR` (city, empty state, country code).
3. Choose your clock preferences, then tap **Save & Restart**. Location lookup supplies the coordinates and timezone. If lookup fails, correct the location and try again.
4. After restart, allow time for network synchronization and the first weather update.

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
| Settings | Tap **Cancel**, or swipe up on the background | Leave without saving changes. |
| Unread message | Tap the displayed message | Mark it read for this clock and return home after acknowledgment succeeds. |

### Weather and storm conditions

The conditions screen combines measurements with a short outlook, such as “Calm and dry” or “Thunderstorms likely by 4 PM.” Storm conditions include strong wind or gusts, thunderstorms, heavy rain or snow, and a sharp pressure drop.

During an alert, a pulsing edge indicator appears on the clock. It is amber during the day and red in Night Shift. Swiping left opens **Conditions first**, then Forecast.

Weather normally refreshes every ten minutes. If an update fails, the clock keeps the last available weather while retrying. That retained weather may be out of date.

### Night Shift

The current **Night Shift On/Off** setting enables a red appearance between **10 PM and 6 AM**. Its schedule is fixed in the current firmware. Adjustable schedules, sunset/sunrise operation, brightness controls, and backlight fades are planned.

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

The current browser's **Active Messages** list shows messages sent to **All clocks**. Messages sent to an individual clock can be dismissed on that clock, but do not appear in this browser list.

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
| Weather location | The place used for weather and the clock's timezone. Enter `City, State`, optionally followed by a country code, such as `Cincinnati,OH,US`. For a city and country without a state, use `Paris,,FR`. |
| Room name | The friendly name shown when choosing message targets. |
| Device ID | The identity used by shared messaging; choose a distinct value for every shared clock. |
| House Messages | **Single** for this clock or **Shared** for several clocks. |
| Clock face | **Analog** or **Digital**. |
| Time format | **12-hour** or **24-hour**. |
| Leading zero | Whether 12-hour time displays `07:00 AM` or `7:00 AM`. |
| Units | **Imperial** or **Metric** weather measurements. |
| Night Shift | Enable or disable the current fixed nighttime appearance. |

**Save & Restart** applies changes and restarts the clock. **Cancel** discards edits. Alarm settings and the expanded night controls will be added with the planned enhancements.

## Development Notes

The firmware is written in C using **ESP-IDF 5.5.5**. **LVGL** draws the touchscreen interface. The firmware project and shell helpers live in `targets/esp32-p4/`.

Read [AGENTS.md](AGENTS.md) before changing the project. The following documents cover the detailed design and development workflow:

| Document | What it covers |
| --- | --- |
| [docs/wac/architecture.md](docs/wac/architecture.md) | Approved behavior, defaults, ownership, source structure, and links to the visual design. |
| [targets/esp32-p4/README.md](targets/esp32-p4/README.md) | Environment preparation, build, flash, monitor, and browser-firmware preparation. |
| [targets/esp32-p4/tests/README.md](targets/esp32-p4/tests/README.md) | Existing checks, how to run them, and verification limits. |

The source structure in the architecture document is a plan; listed enhancement files are created only when needed. The mockup is a visual reference, not firmware or a hardware emulator.

No board is available for verification of this fork yet. A successful build does not establish touchscreen behavior, physical brightness, RTC operation, audible alarms, or behavior during power loss.

## License and attribution

This project is licensed under the [Creative Commons Attribution-NonCommercial 4.0 International License](https://creativecommons.org/licenses/by-nc/4.0/).

Original project © 2026 Josh Hendrickson. [Author](https://anoraker.com). The original project credits [MagicMirror](https://github.com/MagicMirrorOrg/MagicMirror) as inspiration.
