# Development policy

This document owns implementation, review, and verification practices. The [architecture](architecture.md) owns responsibilities and file placement; the [behavior](behavior-contract.md), [storage](storage-contract.md), and [screen-state](screen-state-map.md) contracts own product requirements.

## YAGNI: the smallest complete solution

Choose the simplest complete implementation that satisfies the current approved requirements within the authorized scope. Prefer removing unnecessary code, narrowing existing behavior, or reusing its owner before adding structure. A larger change can be justified when it removes complexity from the resulting design.

- Give substantive new helpers, modules, background tasks, queues, dependencies, settings, and compatibility paths a concrete current purpose. Explain consequential additions and why existing code was insufficient in the completion report.
- Prefer existing ESP-IDF, LVGL, and board facilities when they meet the requirement. Use direct calls where a wrapper would add no meaningful responsibility.
- Keep fixed product choices fixed unless a current requirement calls for configurability. Add planned files only when needed; follow the architecture's file-creation boundary before adding an unlisted source-code path.
- Remove obsolete paths encountered within the authorized change rather than retaining competing implementations. Preserve unrelated work and behavior.
- Split code for distinct responsibilities or lifetimes. File length and caller count alone do not justify splitting or merging.
- Include persistence guarantees, failure handling, and recovery required by the current feature. Simplicity does not permit an incomplete implementation of an agreed contract.

## MECE: clear ownership and complete coverage

Give each decision, authoritative state, and shared resource one owner. Ensure every required behavior has an owner and that affected consumers remain consistent. Use the architecture's ownership assignments without duplicating them here.

- Keep coordination focused on starting services, passing commands, and collecting results. Feature owners retain their decisions.
- Keep defaults, validation, and migration decisions with the configuration owner. Screens edit drafts and consume committed service state according to the contracts.
- Derive displayed values from authoritative state. Copies and snapshots must have a defined source and update path; they must not become competing authorities.
- Give shared helpers a defined responsibility. Separate responsibilities only as far as needed; a module does not automatically require its own background task.
- Follow a changed contract through its affected inputs, validation, persistence, service behavior, presentation, and relevant failure paths. Fix the owning path and its consumers rather than only the first failing example.

## Before a substantive change

Identify the requirement and its owner, affected consumers and states, necessary new structure, and focused checks that will establish completion. Keep this assessment proportional to the change; it may remain in the working conversation and does not require a report file.

Resolve ordinary implementation choices within existing authorization. Ask about unresolved product choices and obtain permission for unlisted source-code paths under the architecture's rule.

## Firmware execution and resources

Preserve the task and resource boundaries defined by the architecture. Keep slow work outside UI callbacks and the display lock, and preserve independent alarm scheduling. Use the board-managed I²C bus.

Define the ownership and lifetime of data passed between tasks. Handle timeout, queue-full, cancellation, and stale-result behavior where those conditions can affect the current operation. Keep resource use bounded and examine memory and stack use when the change affects them. Do not introduce arbitrary limits or additional infrastructure without a concrete requirement.

Use small boundaries around hardware effects when tests need to replace those effects. Add only the separation needed to test the actual behavior.

## Review practices

- Apply the same simplicity and ownership standards to recommendations as to implementation.
- Present an issue as actionable only when it has a credible trigger and a practical consequence. A constructed example demonstrates possibility; it does not alone establish meaningful risk.
- Check affected code for missing behavior, duplicated decisions, obsolete paths, and unnecessary structure. Review the complete affected contract without expanding into unrelated cleanup.
- Treat settled product decisions as requirements until the user changes them. Historical findings do not automatically authorize new features or reopen settled choices.
- Keep optional improvements distinct from defects. Recommendations must not add speculative requirements merely because another design is possible.
- Create the local working documents and final reports needed by the review workflow, including for passing reviews with no actionable findings. These artifacts fall under the architecture's supporting-file allowance and require no separate path approval. Preserve existing evidence and clean up disposable artifacts owned by the current review.

## Verification and completion

Use the [ESP32 instructions](../../targets/esp32-p4/README.md) for build commands and the [test instructions](../../targets/esp32-p4/tests/README.md) for runnable checks and their limits.

- After repository changes, inspect the final diff and added or renamed files for scope, source-code path approval, and documentation consistency. Check affected document links. Inspect untracked source-code files as well as tracked changes; ignored locations do not exempt source code from the approval rule.
- Run focused behavior checks appropriate to the change. Add meaningful tests for new logic using approved files when needed. Documentation-only changes require document review, not unrelated firmware tests.
- Compile firmware when firmware source, dependencies, or build configuration changes. Use the project's pinned ESP-IDF version and inspect size reporting when resource use changes.
- Use hardware observation for claims involving actual RTC operation, sound, touch, brightness, or power-interruption behavior. A successful build or simulated failure test does not establish those hardware results.
- Report source inspection, automated tests, compilation, and hardware observation separately. Identify failed or unrun checks and remaining limits; never describe them as passed.
- Clean up disposable resources created for checks and preserve useful deliverables within the approved file boundary. Do not remove unrelated user files or processes.

There is no custom repository checker in this setup. The file-creation rule is enforced through prior permission and review of changes; focused automation can be proposed later for demonstrated recurring problems.
