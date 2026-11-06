---
name: hestia-implementation-flow
description: Use for Hestia code research and feature implementation when contract definition, model delegation, and deterministic helper scripts improve the workflow. Activate only after ordinary prompt and design review.
---

Write this skill, its references, and Task/Report files in English by default. Keep user-facing dialogue and Q&A in Japanese.

This workflow is for the Hestia repository. The upper-level model handles design decisions and review, lower-level models handle delegated research and edits, and scripts handle deterministic work. Use Task/Report files with brief messages. Verification and TDD are conditional, not mandatory for every change.

## Activation

- First inspect the prompt and relevant design as part of ordinary task preparation, before activating this skill.
- Activate when the upper-level model judges that contract definition or delegation would help, based on scope, API impact, and expected benefit.
- Trivial direct changes do not require this skill or Task/Report files. Make no changes when the request is already satisfied.
- If substantial work is expected, present the scope, proposed split, and model approach, then ask once whether to use the skill. Do not repeat the question within the approved scope.

## References

- [Workflow](references/Workflow.md): roles, handoffs, research, Q&A, implementation, verification, resume, and parallel work.
- [settings.jsonc](settings.jsonc): model selection modes, candidates, and fixed values; comments explain the fields and available values.
- [AutomationPlan](references/AutomationPlan.md): settings parsing, ID lifecycle, JSON metadata, and helper-script contracts. Read only what the current step needs.

See AutomationPlan for script arguments and usage.
