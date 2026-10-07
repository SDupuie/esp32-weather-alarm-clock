# Repository and original firmware baseline

Setup date: October 7, 2026. The specifications and mockups are preserved in this fork; alarm and interface implementation has not started.

## Repository ownership

| Purpose | Repository |
|---|---|
| Our development and default push destination (`origin`) | [SDupuie/esp32-weather-alarm-clock](https://github.com/SDupuie/esp32-weather-alarm-clock) |
| Original project and future update source (`upstream`) | [Canterrain/round-weather-display](https://github.com/Canterrain/round-weather-display) |

The existing fork was renamed from `SDupuie/weather-alarm-clock`. Its Git history and fork relationship are retained. The local development folder remains `ESP32 Weather Alarm Clock`; changing a GitHub name does not require moving that folder.

The development checkout has `remote.pushDefault=origin`. Its `upstream` fetch URL points to Canterrain, with a disabled push URL to prevent accidental upstream pushes. Future updates can be inspected with `git fetch upstream`; fetching does not merge them or change the working files. Review and incorporate selected upstream changes deliberately.

## Preserved original

The pre-change baseline is commit **`d9d621ac8e96e907bf962dfb8a31631ca4dba4e9`**, tagged **`baseline-original-2026-10-07-d9d621a`**. At setup, this commit matched both the fork and upstream `main`.

A separate local clone, `ESP32 Weather Alarm Clock Original`, sits beside the development folder. It is detached at that exact commit, has its own Git objects, points to Canterrain for source reference, and has pushes disabled. Do not pull into or develop inside this preserved checkout.

Its locally excluded `baseline-artifacts/` directory contains the build log, exact configuration and dependency lock, toolchain/version metadata, archived managed components, original source Git bundle, compiled firmware, checksums, and rebuild/flash instructions. These generated artifacts are kept locally rather than added to the development repository's source history.

The original firmware was built with **ESP-IDF 5.5.5**, the **ESP32-P4** target, and the original default configuration for the Waveshare 800 × 800 display and 32 MB flash. Its project version is explicitly pinned to `d9d621a`; creating the archival tag must not change the firmware version string. The generated configuration matches the existing development configuration.

Use the archived binaries to load this specific build later. To rebuild on this Mac, use `baseline-artifacts/rebuild.sh` in the original clone; it selects the installed toolchain and checks the pinned revision. The original convenience setup script can update source or install tools, so it is not the preserved-baseline rebuild procedure.

The flash package includes the bootloader, partition table, application, and their actual flash offsets. Follow its README and verify its checksums before flashing. No board was available during setup, so no flashing, device backup, or hardware evaluation was performed. This archive contains firmware, not a board's saved Wi-Fi credentials or settings. A future full-device backup is useful only if there is existing device state worth retaining.

## Specifications and review records

- [Architecture](architecture.md)
- [Behavior contract](behavior-contract.md)
- [Storage contract](storage-contract.md)
- [Screen and state map](screen-state-map.md)
- [DS3231 agreement](../../shared/spec/ds3231-backup-clock-first-iteration.md)

The mockups contain illustrative data and remain design references. Adversarial review and closure reports remain local under `.reviews/` and are excluded from commits. Machine-specific editor settings also remain local.
