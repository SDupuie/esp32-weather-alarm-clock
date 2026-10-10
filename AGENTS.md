# Project instructions

Build the ESP32-P4 weather alarm clock for the Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C from the original ESP-IDF application. Only ESP32-P4 is supported; maintaining Raspberry Pi or other firmware implementations is out of scope.

There is no backward-compatibility requirement with other firmware or its settings. Do not add migration, compatibility adapters, or alternate UI build modes for it.

Read [docs/architecture.md](docs/architecture.md) before development.

Preserve approved product features and visual decisions. Follow the architecture and explicitly identify any proposed reduction in agreed behavior. Do not silently drop product decisions.

Before each development slice, explicitly identify any proposed change to existing behavior that is not required by a new feature. Explain the current behavior, the proposed change, why it is recommended, and its practical effect. Distinguish it from new-feature implementation and cleanup that preserves behavior. This includes bug fixes, moving existing operations into the background, and changes to failure or retry behavior, even when already described in the approved architecture. Disclose these changes before implementation, not only in a completion report.

Use existing ESP-IDF, LVGL, and board facilities. Reuse and change existing application code when that is simpler. Add modules, tasks, queues, and persistence machinery only for a concrete current need. Give each authoritative state and shared resource one owner; do not duplicate detailed requirements across documents.

Use plain language. Report only credible, consequential audit findings.

Test affected behavior and compile firmware after source changes; distinguish these checks from hardware verification. A board is available; hardware testing is pending. Do not push, merge, or flash without authorization.

## Document authority

- [AGENTS.md](AGENTS.md): agent workflow and this authority map.
- [docs/architecture.md](docs/architecture.md): approved behavior, defaults, accepted tradeoffs, service rules, implementation ownership and source structure; links to the approved visual reference.
- [docs/work.md](docs/work.md): current development tasks, completed slices and the next slice; does not define product or architecture requirements.
- [README.md](README.md): project overview, implementation status, hardware, setup and user guide; links to detailed development instructions.
- [targets/esp32-p4/README.md](targets/esp32-p4/README.md): ESP32-P4 environment preparation, build and flash commands.
- [targets/esp32-p4/tests/README.md](targets/esp32-p4/tests/README.md): runnable test instructions and verification limits.

Keep product and architecture requirements in [docs/architecture.md](docs/architecture.md); link to them rather than restating them in other documents.

## Current checkout and retained material

Work in this project folder on `main`. Integrate useful upstream changes deliberately into this application; do not automatically update or replace the whole project from upstream. Setup scripts must not update the checkout automatically. Preserve unrelated work.

Startup and inherited-screen cleanup is the current development priority. Carry it through bounded, tested slices rather than deferring it to an unspecified future sprint. Read the cleanup scope in the architecture and [docs/work.md](docs/work.md) when resuming work. Update completed and remaining items in docs/work.md before ending a development slice or a review that changes the work list; distinguish unimplemented recommendations from approved work. File moves or lower line counts alone do not establish completion. Keep README.md's implementation-status summary current, without putting a task list there.

Remove superseded plans, obsolete implementations, unused experiments and backup copies when they no longer serve a current purpose within the work being performed. Do not create backup or archive copies of material being retired. Retain material only for an identified current application, development, verification or evaluation need.

Any existing `archive/` directory is excluded from ordinary development searches and implementation audits, including commands that otherwise include ignored files. It may be inspected to assess its current purpose and recommend cleanup, but is not an implementation authority. Nothing current may link to or depend on its contents. Do not restore archived material without explicit user direction. Its existence does not require continued retention or new archival copies.

Current local review reports are allowed as evidence, not requirements. A separate original-firmware evaluation checkout has no automatic retention requirement and is not an implementation authority; retain it only while it has an identified current evaluation purpose.

## File creation and permission

The planned source tree in [docs/architecture.md](docs/architecture.md#planned-file-structure) is the approved list of source-code paths. Listed files are optional; create them only when needed for the current task.

Source code includes firmware, headers, application UI, build logic, development scripts and tools, maintained tests, and generated code incorporated into the application. Putting implementation code in a report, temporary directory or ignored location does not exempt it from this rule.

Before creating or renaming a source-code file to a path outside that list, obtain the user's permission for its exact path and purpose. Explain why an existing approved file is insufficient. Approval of a feature does not automatically approve additional source files. Update the approved tree only after permission is received.

Supporting documents, review reports, logs, screenshots, exported results and disposable test data may be created as needed for authorized work without separate path approval. Reuse existing documents where they own the subject, keep review artifacts local under `.reviews/`, and keep disposable files in the operating system's temporary directory. Supporting files must not become an alternate location for application or tooling source code.

Authorized builds and tests may create ordinary tool-generated outputs, managed dependencies and disposable test directories. This allowance does not approve generated source files added to the maintained application at unlisted paths. Clean up task-owned disposable outputs when finished.

Before completing a change, check added and renamed source files against the approved tree and permissions granted in the conversation. Preserve unrelated work and existing evidence.

## YAGNI and MECE reviews

When presenting choices, explain which is the YAGNI choice, whether each preserves, expands, or narrows the agreed behavior, and the practical tradeoff. Judge the finished design, not effort already spent.

Review both code and documentation. Question whether the structure is needed, not only whether it is internally consistent. For each file, module, abstraction, task, queue, storage mechanism or process within the review scope, ask:

- What concrete current benefit does it provide?
- Could it be removed or combined with something else while preserving approved behavior and clear ownership?
- Does the separation make the project easier to understand, use, test or maintain, or merely organize complexity we created?

A distinct stated responsibility is not sufficient justification for a separate file or mechanism. MECE requires clear, non-overlapping ownership and complete coverage of approved requirements; it does not require a separate document or module for every category.

Prefer the simplest structure that meets current requirements. Do not add machinery for hypothetical future needs, or combine things merely to reduce file count. For a proposed simplification, explain the concrete burden it removes, any tradeoff, and whether approved behavior is preserved. Do not call a design YAGNI solely because consistency checks passed; state the review scope and any limits on that conclusion. Apply these questions to delegated reviewers as well.
