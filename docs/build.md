# ESP32-P4 build and installation

Use this guide to build and install the clock's firmware for the Waveshare
**ESP32-P4-WIFI6-Touch-LCD-3.4C**.

The [architecture and product specification](architecture.md) defines approved behavior, defaults, the visual reference and source ownership. The root [README](../README.md) covers hardware, setup and using the clock.

Building from source means turning the project's editable files into firmware
that runs on the clock. This guide takes you through downloading the project,
preparing the tools, building, and installing it. Copy one command at a time,
press **Return** or **Enter**, and wait for it to finish before entering the
next command.

## Get the project

1. Install [Git](https://git-scm.com/downloads), the program used here to download
   the project.
2. Open a command window after installing Git:
   - **Mac:** press **Command + Space**, type **Terminal**, and press **Return**.
   - **Windows:** open the Start menu, type **PowerShell**, and open it.
   - **Linux:** open **Terminal** from your applications menu.
3. Choose where to download the project. The following examples use your user
   folder. On **Mac or Linux**, enter:

   ```bash
   cd "$HOME"
   ```

   On **Windows PowerShell**, enter this instead:

   ```powershell
   cd "$env:USERPROFILE"
   ```

   `cd` tells the window which folder to work in. `$HOME` and
   `$env:USERPROFILE` stand for your user folder on the respective systems;
   leave those words exactly as written. Use a location without spaces in
   its full folder path; if your user-folder path contains spaces, choose
   another location before downloading. For example, on Windows, create a
   folder named `esp` directly on the `C:` drive using File Explorer, then
   enter `cd C:\esp` in PowerShell instead of the user-folder command above.
4. Enter these commands on any of the three systems:

   ```text
   git clone https://github.com/SDupuie/esp32-weather-alarm-clock.git
   cd esp32-weather-alarm-clock
   ```

   The first command downloads the project into a new folder named
   `esp32-weather-alarm-clock`. The second makes that folder the command
   window's working folder. If Git is not recognized, close and reopen the
   command window after installation before trying again.

## Prepare the environment (no board needed)

**ESP-IDF** is Espressif's toolkit for building software for this board.
Install version **5.5.5** outside the project folder with Espressif's
[ESP-IDF Installation Manager](https://docs.espressif.com/projects/idf-im-ui/en/latest/installation_methods.html).
Select version **5.5.5** and the **ESP32-P4** tools. The installer prepares
Python, the compiler and build tools. Complete its operating-system prerequisite
steps; the platform guides below also cover manual installation.

The tools need a short setup step each time you open a new command window.
This is called **activation**: it tells that window where to find the installed
tools. It does not reinstall them. Follow only the section for your computer,
then continue with **Confirm the environment**.

### Mac

1. Press **Command + Space**, type **Terminal**, and press **Return**.
2. Find the file named **`activate_idf_v5.5.5.sh`** in the tools folder created
   by the ESP-IDF installer. This is a small file containing Espressif's setup
   commands. Use the installation location you chose in the installer.
3. In Terminal, type `source` followed by a **space**. Drag that file from
   Finder into the Terminal window, then press **Return**. Dragging inserts its
   full location, so you do not have to type it.

The word `source` is part of the command: it runs the file's setup instructions
in the current Terminal window, making commands such as `idf.py` available.
For example, **if your installer put the file in `~/.espressif/tools`**, you can
copy this complete command instead:

```bash
source "$HOME/.espressif/tools/activate_idf_v5.5.5.sh"
```

`$HOME` means your user folder; leave it exactly as written. To look in that
example folder in Finder, choose **Go → Go to Folder**, enter
`~/.espressif/tools`, and press **Return**. If the folder or file is absent,
use your installer's chosen location instead. Do not substitute the clock's
project folder.

Continue with [Confirm the environment](#confirm-the-environment) below. If
installation reports missing Apple tools, follow Espressif's
[Mac prerequisites](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32p4/get-started/linux-macos-setup.html#for-macos-users).

When you are ready to install the firmware on the board, its connection name
usually looks like `/dev/cu.usbmodem8331401`. Enter `ls /dev/cu.*` before and
after plugging in the board; the new entry is the one to use.

### Windows

1. After the installation finishes, find the **IDF_PowerShell** shortcut it
   created on your desktop. If there are several versions, choose the one for
   **ESP-IDF 5.5.5**.
2. Double-click that shortcut. It opens a command window with Espressif's tools
   already prepared. Use this window for the remaining commands, rather than
   an ordinary PowerShell window opened from the Start menu.
3. Continue with [Confirm the environment](#confirm-the-environment) below.

If you used Espressif's older Tools Installer instead, open its **ESP-IDF
PowerShell** shortcut for version 5.5.5. See
[Espressif's Windows setup guide](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32p4/get-started/windows-setup.html)
and the installation manager's
[activation instructions](https://docs.espressif.com/projects/idf-im-ui/en/latest/after_installing.html).

The shared `idf.py` commands below work in this window. You do not need to run
the project's `.sh` files on Windows.

Find the board under **Device Manager → Ports (COM & LPT)**. Its entry may say
**USB Serial Device (COM5)**; use the actual `COM` number shown. Unplug and
reconnect the board if needed to identify which entry belongs to it.

### Linux

1. Complete Espressif's
   [prerequisites for your Linux distribution](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32p4/get-started/linux-macos-setup.html#for-linux-users),
   then finish the ESP-IDF installation.
2. Open **Terminal** from your applications menu. These commands use Bash,
   the command interpreter included with many Linux desktops.
3. Find **`activate_idf_v5.5.5.sh`** in the tools folder created by the installer.
   In Terminal, type `source` followed by a **space**, then the full location of
   that file in double quotes, and press **Enter**. The installer creates the
   file; you do not write it yourself.

For example, **if the file is in `~/.espressif/tools`**, copy:

```bash
source "$HOME/.espressif/tools/activate_idf_v5.5.5.sh"
```

This makes Espressif's build commands available in that Terminal window.
`$HOME` means your user folder; leave it exactly as written. If the file is
elsewhere, replace the quoted location with the actual one. Continue with
[Confirm the environment](#confirm-the-environment) below.

#### USB access on Linux

These steps apply to both the Chrome installer and installation from source.
You do not need ESP-IDF for installation from Chrome.

1. Close any other installer or serial monitor connected to the board. Only one
   program can use its serial port at a time.
2. Open **Terminal** and enter this command before and after connecting the board:

   ```bash
   ls /dev/ttyACM*
   ```

   The new entry, usually `/dev/ttyACM0`, is the board. If no entries exist while
   it is unplugged, a **No such file or directory** message is normal. If no
   entry appears when connected, check the USB data cable and connection.
3. If opening the port reports **Permission denied**, give your account access
   to serial devices. On Ubuntu, Debian and many other Linux distributions, enter:

   ```bash
   sudo usermod -a -G dialout "$USER"
   ```

   On Arch Linux, use this instead:

   ```bash
   sudo usermod -a -G uucp "$USER"
   ```

   Leave `"$USER"` exactly as written. It means your current username. If prompted,
   enter your computer account's password. Characters do not appear as you type it.
4. Sign out of your Linux desktop and sign back in so the new access takes effect.
   Reopen Chrome or your build Terminal, reconnect the board, and try again.

See [Espressif's Linux serial-access instructions](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32p4/get-started/establish-serial-connection.html#adding-user-to-dialout-or-uucp-on-linux)
for details.

### Confirm the environment

In the window you prepared above, enter:

```text
idf.py --version
```

It should print **ESP-IDF v5.5.5**. If it says `command not found` or that
`idf.py` is not recognized, the tools are not ready in that window; repeat the
steps for your computer above.

Next, tell the window to work in the downloaded project folder:

- **Mac or Linux:** type `cd` and a space, followed by the full project-folder
  location in double quotes, then press **Return/Enter**. On Mac, you can drag
  the folder from Finder after typing `cd` and a space to insert its location.
- **Windows:** open the project folder in File Explorer, click the address bar,
  and copy the full location. In the prepared PowerShell window, type `cd` and
  a space, paste that location inside double quotes, and press **Enter**.

For example, these commands work **if you downloaded the project directly into
your user folder**. Choose only the line for your computer:

```bash
cd "$HOME/esp32-weather-alarm-clock"
```

```powershell
cd "$env:USERPROFILE\esp32-weather-alarm-clock"
```

The first example is for Mac/Linux; the second is for Windows PowerShell.
Check that you are in the correct folder by entering `ls`. You should see
`CMakeLists.txt`, `sdkconfig.defaults`, `main`, and `assets`. This is what the
other instructions mean by the **repository root** or **project root**.
Keep using this window for building and installing the firmware. A new window
needs the preparation steps again.

If you installed ESP-IDF manually rather than using the installation manager,
its setup file is named `export.sh`; use that file in the Mac/Linux steps.
Developer tests also use the `IDF_PATH` value supplied by this preparation to
find cJSON at `$IDF_PATH/components/json/cJSON`.

## Build without a board

Before the **first build of your downloaded project**, enter this in the
prepared window while it is working in the project folder:

```text
idf.py set-target esp32p4
```

This selects the ESP32-P4 processor and creates the build settings in a file
named `sdkconfig` on your computer. The supplied `sdkconfig.defaults` contains
the board's other settings. Do not repeat this command for ordinary rebuilds:
it clears the existing build and resets its settings to the supplied defaults.

Then build with the same command on Mac, Windows or Linux:

```text
idf.py build
```

Wait for **Project build complete** before continuing. The first build needs
internet access to download the additional software the project uses. These
downloads are selected using
`main/idf_component.yml` and `dependencies.lock`. ESP-IDF fetches them
automatically into `managed_components/`. The build creates its outputs under
`build/`; a board is not required.

Compiled artwork and fonts are already included in the project. An ordinary
build does not require CairoSVG, artwork regeneration or audio conversion.

The build also creates the image containing all nine Sound Machine recordings;
the normal installation command below includes it automatically.

See the [test instructions](../tests/README.md) for native software checks.

## Flash with a board

Connect the board using a USB-C data cable. Close Chrome's installer and any
other program using its serial port—the connection used to send firmware from
the computer to the board. The port name depends on your computer:

| Computer | Example port | Example flash command |
| --- | --- | --- |
| Mac | `/dev/cu.usbmodem8331401` | `idf.py -p /dev/cu.usbmodem8331401 flash` |
| Windows | `COM5` | `idf.py -p COM5 flash` |
| Linux | `/dev/ttyACM0` | `idf.py -p /dev/ttyACM0 flash` |

After preparing the tools and building, enter these commands. Replace `PORT`
with the actual name from your computer; do not type the word `PORT` unchanged.

```text
idf.py -p PORT flash
idf.py -p PORT monitor
```

The `flash` command installs the firmware and recordings, then restarts the
board. It includes all four required images: the bootloader, partition table,
application and recordings. Ordinary flashing does not erase saved settings;
do not add `erase-flash` when updating an existing clock.

The optional `monitor` command displays messages from the running board. Hold
**Ctrl** and press **]** to close it and release the connection. If the board
cannot enter download mode automatically, follow
[Espressif's USB flashing instructions](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32p4/get-started/establish-serial-connection.html#flash-using-usb)
to use its BOOT and RESET buttons. After a first installation, complete
[touchscreen setup](../README.md#finish-setup-on-the-touchscreen).

## Optional development tools and diagnostics

VS Code is optional. If you use it, install Espressif's
[ESP-IDF extension](https://github.com/espressif/vscode-esp-idf-extension) and open
the project folder as the firmware project.

The `scripts/*.sh` helpers are for a Bash environment on Mac or Linux. Once
ESP-IDF is activated, `scripts/build.sh`, `scripts/flash.sh PORT` and
`scripts/monitor.sh PORT` wrap the shared commands above.
`scripts/set-target.sh` selects ESP32-P4 and resets the local build configuration,
just like `idf.py set-target esp32p4`.

Alternatively, `scripts/setup.sh` resets the target configuration, builds,
detects the connected board, and flashes it. It optionally opens the monitor.
Use it when that reset and flash are intended. It does not install a toolchain
or update repository source.

For a deliberate whole-device backup before flashing, run `scripts/backup-device.sh <serial-port> [output-dir]` from the repository root. It defaults to the ignored `backups/<timestamp>/` directory at the repository root. Keep backups local. This is a manual hardware tool, separate from application settings saves.

For a diagnostic serial capture, run `scripts/capture-serial-log.sh <serial-port> <output-path> [duration-seconds]` from the repository root. It captures for the requested duration (150 seconds by default). A successful capture means the log was collected; it does not mean the firmware passed a benchmark or hardware test.

That wrapper requests a restart before capture. To preserve an already frozen state, use `python tools/capture_serial.py --port <serial-port> --output <output-path> --duration 150` from the repository root, without `--reset-before-capture`; do not reset or flash before collecting the stopped-state evidence.

## Prepare browser-installation files

This optional packaging helper requires Bash on Mac or Linux. After selecting
ESP32-P4 and activating the environment above, run from the repository root:

```bash
scripts/build-release.sh
```

This builds through the normal build helper and copies four firmware images into
`docs/firmware/`: bootloader, partition table, application and `storage.bin` with
the nine Sound Machine recordings. The installer manifest includes all four at
the current partition offsets. It does not publish the website or flash a board.

For a versioned release, update the version in `docs/firmware/manifest.json` manually. The helper does not choose or change it.

## Regenerate artwork (optional)

These Bash tools are for changing artwork; they are not part of a normal source
build. Regenerate the 18 weather pictures from the SVG sources in `assets/icons/`,
including crop metadata, with `PYTHON_BIN=<CairoSVG-environment-python> bash scripts/generate-screen-parity-assets.sh --weather-only`. The environment needs CairoSVG;
this focused run updates weather image data and placement metadata together.
For the shared settings and utility icons, use the same command with
`--settings-only`. That mode updates utility icons in the combined icon outputs
while retaining the weather image bytes and crop metadata.
For the three Analog hands, use the same environment with `bash scripts/generate-analog-parity-assets.sh --hands-only`; the existing hand geometry and transparency are retained. Image
format and placement rules belong in the [architecture](architecture.md#weather-icons).

Artwork generators update existing outputs. When adding or retiring a generated source, update `main/CMakeLists.txt` under the [source-list rules](architecture.md#board-and-build-integration); there is no separate generated build manifest.

Run either artwork generator without its focused flag to regenerate its complete
set. Background and Forecast-frame SVGs are created alongside temporary PNGs and
removed when the script exits; they are not maintained under `assets/`.
Maintained input locations and
compiled output ownership follow the [artwork storage rules](architecture.md#artwork-storage).
Full Analog generation also needs NumPy and Pillow for the existing edge artwork.

## Build configuration notes

The checked-in configuration uses the BSP's returned-error option so speaker setup failures can be reported without its optional whole-device abort. This also affects checked display/touch/bus/backlight failures; it is not a promise that every board failure is recoverable. Healthy paths and the standard touch/display setup are preserved.

The application uses the board's 32 MB flash, including addresses above 16 MiB.
The configuration enables ESP-IDF's extended cache-address option and its
experimental-feature gate; setting the flash size alone does not enable those
reads. Keep the matching bootloader and application together when flashing.
The build also generates `storage.bin` from the nine prepared Sound Machine MP3s
in `assets/sounds/clock/`. Ordinary USB flashing includes that image in
the existing storage partition; it does not erase NVS settings. Source recordings
are not packaged. Playback uses the managed `esp_audio_codec` dependency pinned
to 2.5.0 for the development board's revision 1.3; newer releases require newer
P4 silicon. Its package retains its own license and notices.

## Verified Baseline

- Waveshare recommends `ESP-IDF` for the `ESP32-P4-WIFI6-Touch-LCD-XC` series.
- Waveshare documents the `3.4C` as the default `800x800` round display variant in the `XC` BSP.
- Waveshare recommends `ESP-IDF v5.5.0` or newer for the `ESP32-P4-WIFI6-Touch-LCD` line; this project is pinned to `v5.5.5`.

Sources:

- <https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-XC>
- <https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-X/Development-Environment-Setup-IDF>
- <https://components.espressif.com/components/waveshare/esp32_p4_wifi6_touch_lcd_xc/versions/3.0.0/readme>

## Project Layout

One `main` component holds the application, grouped by function. UI files use `main/ui/`; assets use `main/assets/`. The [approved source tree](architecture.md#planned-file-structure) also identifies optional files that are created only when needed.

## Notes

- The project is set up to use the Waveshare `XC` BSP with the `3.4C` round display as the intended default.
