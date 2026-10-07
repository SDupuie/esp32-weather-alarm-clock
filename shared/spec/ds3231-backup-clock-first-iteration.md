# DS3231 Backup Clock — First Iteration

Accepted behavior: October 6, 2026. This document records the agreed design;
the external RTC is not yet implemented in the firmware. The screen mockup
illustrates the notice presentation proposed on October 7, 2026.

## Time Source

- Network time is the primary source whenever available.
- The DS3231 supplies backup time after a power interruption, until network
  time is available, only if its stored time is trustworthy.
- A plausible date alone must not prevent the network time service from
  starting. Update the RTC after successful network synchronization.

## Startup and Recovery

1. Read the DS3231 oscillator stop flag (OSF) before any network time update
   writes to the RTC or clears its flag.
2. If the flag is set, treat the RTC's stored time as unreliable. Do not use
   that time to initialize the application clock or trigger an alarm.
3. Record a persistent, dismissible notice:

   **Backup Clock Lost Time — Check Battery.**

4. Obtain network time and set the RTC to that time. Keep the oscillator
   enabled for battery operation.
5. After successfully setting the RTC, clear its stop flag and allow its
   corrected time to be used as a backup. Retain the notice until the user
   dismisses it.

If network time is unavailable, keep waiting for synchronization and retain
the notice. If setting the RTC fails, keep it marked unreliable and do not
clear its stop flag.

## Notice Lifetime

- Save the notice before correcting the RTC. Store it independently of the
  hardware flag so a successful correction or application restart cannot
  erase it.
- Dismissing the notice acknowledges the earlier loss of time; it does not
  establish that the battery has been replaced or is healthy.
- A corrected RTC can be running normally while the earlier notice remains
  visible. The notice must not prevent normal clock or alarm operation once
  trustworthy time is available.
- If another oscillator stop is detected after dismissal, record the notice
  again. A failed attempt to read the RTC is a connection/read failure, not
  evidence that the stop flag is set.

## Interpretation and Scope

The stop flag reports that timekeeping was interrupted. It does not prove a
dead battery: first use and other oscillator interruptions can also set it.
This iteration detects a loss of backup time; it does not provide advance
low-battery detection, battery voltage, or remaining capacity. No battery
measurement circuitry is part of this iteration.

The screen mockup uses a warning indicator beside the day/date on both
clock faces. The notice opens only when the user taps the indicator; it
does not open automatically when backup time is lost. Tapping the indicator
opens a compact notice over the clock, containing the loss-of-time message
and a single DISMISS button. DISMISS closes the
notice and clears its warning indicator. There is no separate close action
that leaves the warning pending, and tapping outside the notice does not
close it. Escape in the keyboard preview performs the same acknowledgment
as DISMISS. The notice uses the existing day/night styling and dismiss-button size.
There is no separate Backup Clock settings or diagnostics screen for this
iteration.

For a failed RTC read or update, the same indicator and notice window show
**Backup Clock Unavailable — Check Connection.** This is a separate reason
from the oscillator-stop warning. It uses the same tap-to-open and DISMISS
behavior; dismissal acknowledges the notice and does not establish that
backup time is trustworthy. Network time remains the primary time source.
The preview provides choices for each notice reason, either open or shown
as its warning indicator.

The indicator is separate from the alarm and missed-alarm controls. An
active ringing alarm takes priority over an open notice; the backup notice
remains pending when the alarm screen is shown.

## Existing Startup Integration

In `targets/esp32-p4/main/connectivity.c`, `connectivity_sync_time()` currently
returns before starting the network time service if the system date looks
valid. `targets/esp32-p4/main/main.c` also uses that date check to decide
whether synchronization is needed. When RTC support is implemented, keep
the validity of backup time separate from whether network synchronization
has actually occurred, and start the network time service even when a
trusted RTC has supplied the initial date.

## Review Checks for Implementation

- With the stop flag set, startup records the notice before any RTC update
  and ignores the stored RTC time.
- Successful network synchronization corrects the RTC and clears the flag,
  while the notice remains pending through a restart until dismissed.
- Without network time, an untrusted RTC does not supply clock or alarm time.
- With the stop flag clear and a valid RTC time, startup can use backup time
  while still starting network synchronization.
- An RTC read or write failure does not incorrectly mark backup time trusted,
  and shows Backup Clock Unavailable — Check Connection rather than inferring
  that the oscillator-stop flag is set.
- After dismissal, a subsequent detected stop creates a new notice.

These are planned checks, not results of hardware tests.

## Hardware Reference

[Analog Devices DS3231 datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/DS3231.pdf),
Control Register (0Eh), EOSC, and Status Register (0Fh), OSF. OSF remains set
until software clears it; the application must read it before correcting the
RTC.
