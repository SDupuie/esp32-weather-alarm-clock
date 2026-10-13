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

- Analog and digital clock faces, with 12-hour or 24-hour time and an optional leading zero.
- Large, centered digital time in Night Shift and a common unavailable-time screen for either clock face.
- Current temperature, today's high and low, and weather icons that change between day and night.
- A forecast for the next five days, with icons chosen to represent the daytime weather.
- A conditions screen with wind, gusts, pressure, rain or snow, and a short weather outlook.
- An edge indicator for approaching storm conditions, with quick access to the conditions screen.
- Weather from Open-Meteo without a key, or Visual Crossing with your API key.
- Time synchronized over Wi-Fi.
- DS3231 backup-clock support for startup while network synchronization proceeds, with lost-time and connection notices.
- A red Night Shift appearance with Off, Always On, Fixed Schedule or locally calculated Sunset to Sunrise preferences, remembered startup fallback, and separate Day/Night brightness with gradual transitions.
- Household messages sent from a phone or computer on the same network, with optional sharing among multiple ESP32-P4 clocks.
- Touchscreen settings for Wi-Fi, location, display, messaging and saved alarm/night preferences.
- One recurring alarm with selected repeat days, Snooze, Dismiss, missed-alarm notices and restart recovery.
- Classic Beep, Classic Bell, and Rising Tones, with a five-second sound preview and gradually increasing alarm volume.
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

For firmware updates, build and flash again over USB. The browser installer files in [docs/index.html](docs/index.html) currently package the original weather-display baseline, without this project’s alarm-clock enhancements.

## Using the Clock

Your selected analog or digital face is the home screen.

| From | Gesture or control | What happens |
| --- | --- | --- |
| Clock | Swipe left | Open Forecast, then Conditions with another left swipe. |
| Forecast or Conditions | Swipe right | Step back toward the clock. |
| Clock | Swipe right | Open Messages. |
| Messages | Swipe left | Return to the clock. |
| Clock | Swipe down | Open settings. |
| Settings editor | Tap **Cancel** | Discard that editor's unsaved changes; Weather returns to Device Setup, while Display, Device Setup and Alarm return to Settings. |
| Settings entry | Tap **Back** | Return to the clock after first-use location setup is complete. |
| Unread message | Tap the displayed message | Mark it read for this clock and return home after acknowledgment succeeds. |

### Weather and storm conditions

The conditions screen combines measurements with a short outlook, such as “Calm and dry” or “Thunderstorms likely by 4 PM.” Storm conditions include strong wind or gusts, thunderstorms, heavy rain or snow, and a sharp pressure drop.

During an alert, a pulsing edge indicator appears on the clock. It is amber during the day and red in Night Shift. Swiping left opens **Conditions first**, then Forecast.

Weather normally refreshes every ten minutes. If an update fails, the clock keeps the last available weather while retrying. That retained weather may be out of date.

In **Device Setup → Weather**, choose **Open-Meteo** without a key or
**Visual Crossing** with your API key. The [configuration guide](#configuration)
explains activation checks, key retention and clearing.

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
| Weather provider / API key | Choose Open-Meteo or Visual Crossing in **Device Setup → Weather**. Visual Crossing requires a key, checked when first selected or changed. |
| Room name | The friendly name shown when choosing message targets. |
| Device ID | The identity used by shared messaging; choose a distinct value for every shared clock. |
| House Messages | **Single** for this clock or **Shared** for several clocks. |
| Clock face | **Analog** or **Digital**. |
| Time format | **12-hour** or **24-hour**. |
| Leading zero | Whether 12-hour time displays `07:00 AM` or `7:00 AM`. In 24-hour mode it shows On and cannot be changed; switching back restores the previous 12-hour choice. |
| Units | **Imperial** or **Metric** weather measurements. |
| Brightness | Day/Night levels under **Settings → Display Settings → Brightness**. |
| Night Shift | Mode, fixed schedule, and solar offsets with calculated start/end times. |
| Alarm | Time, repeat days, sound, volume, snooze and On/Off. The editor previews the next alarm for unsaved time/day edits. |

Display Settings has its own **Cancel** and **Save**. Save applies the clock face, time format, leading zero and Day/Night brightness without restarting, then returns to Settings. Cancel or Back discards those edits.

Conditions descriptions also use the saved time format, including while showing weather retained after a failed update.

Device Setup and Wi-Fi share a draft for **Save & Restart**. The **Weather** entry opens the Weather editor for location, provider, units and API key, which save independently and restart after success. Its Back or Cancel returns to Device Setup with any other device edits retained; Weather Save applies only the weather choices. Alarm **Save** applies settings without restarting. The **Alarm On/Off** switches in Settings and Alarm Settings save immediately; **Cancel** does not undo a successful switch change. Other alarm edits still require **Save**. Nested **Done** keeps edits in the parent draft; **Cancel** discards unsaved edits. See the [architecture](docs/architecture.md#editing-and-saving-user-settings) for save rules and accepted partial-save behavior.

Visual Crossing's API key stays masked. **Configured** means a key is saved;
**Entered (not saved)** means you changed it in the editor. Save checks a newly
selected Visual Crossing provider or changed key before applying it. If the
check fails, the editor stays open for retry without restarting. Choosing
Open-Meteo keeps the saved key for later reuse. To remove it, open API Key while
Visual Crossing is selected, tap **Clear**, close the keyboard, choose
Open-Meteo and Save. Cancel discards unsaved key changes.

If Visual Crossing confirms that the key's daily allowance is exhausted, the
clock keeps its last weather and waits for the provider's retry time, or checks
once an hour when none is supplied. You can choose another provider yourself.

Tap a text field to open the keyboard. The active input stays visible above it; Done or Close returns to the menu with your unsaved text. Passwords are masked.

Alarm Time and Fixed Schedule minute arrows move in five-minute steps and change the hour when minutes wrap. Opening an editor leaves the saved time unchanged. Volume and brightness sliders have a thin bar and a large circular handle.

Preview Sound plays the selected draft sound at the draft volume without saving either choice. Stop Preview, leaving the sound picker or a ringing alarm stops the preview. Sunrise/Sunset shows calculated event times when the clock has usable time and a saved location.

## Documentation

See the [firmware setup and update guide](targets/esp32-p4/README.md) for detailed installation instructions and the [approved design](docs/architecture.md) for product behavior. The [weather mapping reference](docs/weather.md) shows our icons, both providers' mappings and source limitations. Development work and verification records are kept in [docs/work.md](docs/work.md).

## License and attribution

This project is licensed under the [Creative Commons Attribution-NonCommercial 4.0 International License](https://creativecommons.org/licenses/by-nc/4.0/).

Original project © 2026 Josh Hendrickson. [Author](https://anoraker.com). The original project credits [MagicMirror](https://github.com/MagicMirrorOrg/MagicMirror) as inspiration.
