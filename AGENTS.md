# Project instructions

Develop the ESP-IDF weather alarm clock application for the Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C. Only ESP32-P4 is supported; maintaining Raspberry Pi or other firmware implementations is out of scope.

There is no backward-compatibility requirement with other firmware or its settings. Do not add migration, compatibility adapters, or alternate UI build modes for it.

Read [docs/architecture.md](docs/architecture.md) before development.

Preserve approved product features and visual decisions. Follow the architecture and explicitly identify any proposed reduction in agreed behavior. Do not silently drop product decisions.

Before each development slice, explicitly identify any proposed change to existing behavior that is not required by a new feature. Explain the current behavior, the proposed change, why it is recommended, and its practical effect. Distinguish it from new-feature implementation and cleanup that preserves behavior. This includes bug fixes, moving existing operations into the background, and changes to failure or retry behavior, even when already described in the approved architecture. Disclose these changes before implementation, not only in a completion report.

Use existing ESP-IDF, LVGL, and board facilities. Reuse and change existing application code when that is simpler. Add modules, tasks, queues, and persistence machinery only for a concrete current need. Give each authoritative state and shared resource one owner; do not duplicate detailed requirements across documents.

Use plain language. Report only credible, consequential audit findings.

## Document authority

- [AGENTS.md](AGENTS.md): agent workflow and this authority map.
- [docs/architecture.md](docs/architecture.md): approved behavior, defaults, accepted tradeoffs, service rules, implementation ownership and source structure; links to the approved visual reference.
- [docs/alarm-design.html](docs/alarm-design.html): approved device and House Messages appearance and controls, subject to the exceptions identified in the architecture. Preview samples do not define product defaults.
- [docs/weather.md](docs/weather.md): detailed provider-to-condition/icon mappings, artwork previews, selection rules, source limitations and current firmware mapping; linked from the architecture.
- [README.md](README.md): user-facing project overview, features, hardware, setup, user guide and source credits; links to detailed development instructions.
- [docs/build.md](docs/build.md): ESP32-P4 environment preparation, build and flash commands.
- [tests/README.md](tests/README.md): runnable test instructions and verification limits.

Link to each subject's owner rather than duplicating detailed requirements. The README explains how to use the implemented features; it is not a separate specification. Distinguish proposals, approved behavior and completed implementation. Recording an unresolved choice does not approve it.

## Current checkout and retained material

Work in this project folder on `main`. Integrate useful upstream changes deliberately into this application; do not automatically update or replace the whole project from upstream. Setup scripts must not update the checkout automatically. Preserve unrelated work.

Carry authorized work through bounded, tested changes. File moves or lower line counts alone do not establish completion. Do not reopen closed checks merely to expand coverage.

Update the documents that own affected subjects before completing a change. Keep README.md user-facing; do not put development status, work lists or test reports there.

Test affected behavior and compile firmware after source changes. A board is available; do not commit, push, merge, or flash without the user's authorization. Keep completed-work, test and flash evidence with its qualifications in local reports under `reviews/`, which is visible and excluded from Git. Reports are evidence rather than requirements. Keep test-method limits in the test instructions. Distinguish software checks, debugger observations, physical board checks and accepted verification limits; general test caveats are not unfinished work.

Keep reports only while they support uncommitted changes awaiting review or commit, an unresolved finding or ongoing test, or a specific current verification need. State the retention reason and the event that permits deletion in each retained report or data folder's README.

Reusable test inputs and expected results belong at approved `tests/data/` paths and are versioned with their tests. Do not keep maintained test dependencies in `reviews/`.

- At task completion, remove disposable captures, scratch files and superseded reports. Keep raw logs and output images only while they support retained evidence or an ongoing investigation.
- After an authorized commit, remove completed reports supporting that committed work unless a stated retention reason remains. Do not commit merely to trigger cleanup.
- Remove empty review folders. Preserve unrelated files and ongoing test evidence. Perform this cleanup as part of normal task and commit completion; `.gitignore` does not clean files up.

Remove superseded plans, obsolete implementations, unused experiments and backup copies when they no longer serve a current purpose within the work being performed. Do not create backup or archive copies of material being retired. Retain material only for an identified current application, development, verification or evaluation need.

Exclude retired or archived material from ordinary development searches and implementation audits. Inspect it only to assess its purpose or when directed by the user; do not use it as an implementation authority, link current material to it, depend on it or restore it without explicit user direction. Evaluation material has no automatic retention requirement.

## File creation and permission

The planned source tree in [docs/architecture.md](docs/architecture.md#planned-file-structure) is the approved list of source-code paths. Listed files are optional; create them only when needed for the current task.

Source code includes firmware, headers, application UI, build logic, development scripts and tools, maintained tests, and generated code incorporated into the application. Putting implementation code in a report, temporary directory or ignored location does not exempt it from this rule.

Before creating or renaming a source-code file to a path outside that list, obtain the user's permission for its exact path and purpose. Explain why an existing approved file is insufficient. Approval of a feature does not automatically approve additional source files. Update the approved tree only after permission is received.

Supporting documents, review reports, logs, screenshots, exported results and disposable test data may be created as needed for authorized work without separate path approval. Reuse existing documents where they own the subject, keep review artifacts local under `reviews/`, and keep disposable files in the operating system's temporary directory. Supporting files must not become an alternate location for application or tooling source code.

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
