# Project instructions

Build the ESP32-P4 weather alarm clock for the Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C from the original ESP-IDF application. Only ESP32-P4 is supported; maintaining Raspberry Pi or other firmware implementations is out of scope.

Read [docs/wac/architecture.md](docs/wac/architecture.md) before development.

Preserve approved product features and visual decisions. Follow the architecture and explicitly identify any proposed reduction in agreed behavior. Do not silently drop product decisions.

There is no backward-compatibility requirement with other firmware or its settings. Do not add migration, compatibility adapters, or alternate UI build modes for it.

Use existing ESP-IDF, LVGL, and board facilities. Reuse and change existing application code when that is simpler. Add modules, tasks, queues, and persistence machinery only for a concrete current need. Give each authoritative state and shared resource one owner; do not duplicate detailed requirements across documents.

When presenting choices, explain which is the YAGNI choice, whether each preserves, expands, or narrows the agreed behavior, and the practical tradeoff. Judge the finished design, not effort already spent.

Use plain language. Report only credible, consequential audit findings. Preserve unrelated work. Test affected behavior and compile firmware after source changes; distinguish these checks from hardware verification. No board is available yet. Do not push, merge, or flash without authorization.

## YAGNI and MECE reviews

Review both code and documentation. Question whether the structure is needed, not only whether it is internally consistent. For each file, module, abstraction, task, queue, storage mechanism or process within the review scope, ask:

- What concrete current benefit does it provide?
- Could it be removed or combined with something else while preserving approved behavior and clear ownership?
- Does the separation make the project easier to understand, use, test or maintain, or merely organize complexity we created?

A distinct stated responsibility is not sufficient justification for a separate file or mechanism. MECE requires clear, non-overlapping ownership and complete coverage of approved requirements; it does not require a separate document or module for every category.

Prefer the simplest structure that meets current requirements. Do not add machinery for hypothetical future needs, or combine things merely to reduce file count. For a proposed simplification, explain the concrete burden it removes, any tradeoff, and whether approved behavior is preserved. Do not call a design YAGNI solely because consistency checks passed; state the review scope and any limits on that conclusion. Apply these questions to delegated reviewers as well.

## File creation and permission

The planned source tree in [docs/wac/architecture.md](docs/wac/architecture.md#planned-file-structure) is the approved list of source-code paths. Listed files are optional; create them only when needed for the current task.

Before creating or renaming a source-code file to a path outside that list, obtain the user's permission for its exact path and purpose. Explain why an existing approved file is insufficient. Approval of a feature does not automatically approve additional source files. Update the approved tree only after permission is received.

Source code includes firmware, headers, application UI, build logic, development scripts and tools, maintained tests, and generated code incorporated into the application. Putting implementation code in a report, temporary directory or ignored location does not exempt it from this rule.

Supporting documents, review reports, logs, screenshots, exported results and disposable test data may be created as needed for authorized work without separate path approval. Reuse existing documents where they own the subject, keep review artifacts local under `.reviews/`, and keep disposable files in the operating system's temporary directory. Supporting files must not become an alternate location for application or tooling source code.

Authorized builds and tests may create ordinary tool-generated outputs, managed dependencies and disposable test directories. This allowance does not approve generated source files added to the maintained application at unlisted paths. Clean up task-owned disposable outputs when finished.

Before completing a change, check added and renamed source files against the approved tree and permissions granted in the conversation. Preserve unrelated work and existing evidence.

## Document authority

- [AGENTS.md](AGENTS.md): agent workflow and this authority map.
- [README.md](README.md): project overview, implementation status, hardware, setup and user guide; links to detailed development instructions.
- [targets/esp32-p4/README.md](targets/esp32-p4/README.md): ESP32-P4 environment preparation, build and flash commands.
- [targets/esp32-p4/tests/README.md](targets/esp32-p4/tests/README.md): runnable test instructions and verification limits.
- [docs/wac/architecture.md](docs/wac/architecture.md): approved behavior, defaults, accepted tradeoffs, service rules, implementation ownership and source structure; links to the approved visual reference.

Keep product and architecture requirements in [docs/wac/architecture.md](docs/wac/architecture.md); link to them rather than restating them in other documents.

## Current checkout and retained material

Work in this project folder on `main`. Setup scripts must not update the checkout automatically. Remove superseded plans, obsolete implementations, unused experiments and backup copies when they no longer serve a current purpose within the work being performed. Do not create backup or archive copies of material being retired. Preserve unrelated work and retain material only for an identified current application, development, verification or evaluation need.

Any existing `archive/` directory is excluded from development, searches and audits, including commands that otherwise include ignored files. Nothing current may link to or depend on its contents. Do not read or restore it without explicit user direction. Its existence does not require continued retention or new archival copies.

Current local review reports are allowed as evidence, not requirements. A separate original-firmware evaluation checkout has no automatic retention requirement and is not an implementation authority; retain it only while it has an identified current evaluation purpose.
