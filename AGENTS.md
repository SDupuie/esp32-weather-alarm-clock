# Project instructions

## Scope and startup

Develop the ESP32-P4 weather alarm clock for the Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C using the existing ESP-IDF application. Raspberry Pi development is outside this fork's enhancement scope.

Read this file at the start of work and after a model change or context recovery. Before resuming edits, inspect the relevant current files and working-tree changes rather than relying on remembered contents. Preserve unrelated user work.

## Authority and required reading

| Document | Authority and when to read |
|---|---|
| [Development policy](docs/wac/development.md) | Implementation, review, and verification practices; read before implementation or review. |
| [Architecture](docs/wac/architecture.md) | Responsibilities, integration points, and the approved new-file tree; read before implementation, architecture review, or file creation. |
| [Behavior contract](docs/wac/behavior-contract.md) | Product behavior; read for affected feature changes or reviews. |
| [Storage contract](docs/wac/storage-contract.md) | Saving, recovery, defaults, and compatibility; read for settings or persistence work. |
| [Screen-state map](docs/wac/screen-state-map.md) | Approved screens and presentation states; read for UI work. |
| [ESP32 development instructions](targets/esp32-p4/README.md) and [test instructions](targets/esp32-p4/tests/README.md) | Setup, build, and verification commands; read for the applicable operation. |
| [Repository setup](docs/wac/repository-setup.md) | Repository ownership and preserved original firmware; read for upstream or baseline work. |

For documentation-only work, read the affected documents and their owners. This file owns agent workflow and routing. The linked documents own their detailed rules; update the owning document instead of copying policy into another file.

The fork contracts govern its enhancements. Mockups, historical reviews, fixtures, and test results do not independently establish requirements or defaults. Keep planned behavior distinct from implemented and verified behavior. Resolve conflicting documents at their source; seek direction when an unresolved product choice affects the task.

## File creation and authorized work

Follow the [file-creation boundary](docs/wac/architecture.md#file-creation-boundary): new or renamed source-code files at unlisted paths require prior permission. Supporting documents and review artifacts may be created as needed for authorized work under that rule.

Carry authorized work through completion. Resolve routine engineering choices within that scope; internal engineering checks do not introduce additional permission gates. Reviews may produce local review artifacts; changes to the material being reviewed require authorization. Keep build, flash, commit, and publish operations within the requested scope.

## Completion

Review the final changes, including added and renamed files, against the approved scope and file-creation boundary. Explain substantive changes and why added structure was needed. Report verification actually performed and material limits, following the development policy.
