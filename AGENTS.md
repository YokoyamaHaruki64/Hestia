# Hestia Repository Guidelines

These guidelines apply throughout the repository. Keep project-wide design details in `Docs/` instead of duplicating shared working policies here.
Use Japanese for user-facing responses, questions, and Q&A. Keep the design documents in `Docs/` primarily in Japanese.

## Design References

- Read [Architecture](Docs/Architecture.md) for the overall structure, [DesignSources](Docs/DesignSources.md) for source provenance, and [OpenDecisions](Docs/OpenDecisions.md) for unresolved decisions.
- When refining the design, start with [DevelopNotes](Reference/HestiaDesign/DevelopNotes/Overview.md) and cross-check the topic-specific material for the relevant System.
- Do not treat alternatives or supplemental drafts as decisions. If documentation and implementation differ, identify the difference and rationale before changing code.

## Coding Standard

- Before creating, modifying, or reviewing code, ensure the relevant rules from [CodingStandard](Docs/CodingStandard.md) and [CodingExamples](Docs/CodingExamples.md) are available in the current context and apply them. Read the documents if needed; otherwise, do not reread unchanged content.
- The include-path policy is undecided. Until it is settled, follow the existing style in each project.

## Implementation Rules

- Place Engine-internal types and types shared across the DLL boundary in `Hestia`; place Game Script-facing facades in `HestiaGame`.
- Make responsibilities, ownership, non-owning references, and destruction order clear. Expose only the operations callers need through public APIs and DLL boundaries.
- Separate functions and distinct groups of work with blank lines. Add concise Japanese comments only where assumptions or rationale are difficult to understand from the code.
- Follow the repository-root `.editorconfig` for line endings, indentation, and encoding.

## Implementation Workflow

- As part of ordinary task preparation, inspect the prompt and relevant design, then decide which steps and delegation are useful.
- Use the repository skill [hestia-implementation-flow](.agents/skills/hestia-implementation-flow/SKILL.md) when contract definition or delegated research/editing would help. If substantial work is expected, present the scope, proposed split, and model approach, then ask once before using the skill.
- The skill and Task/Report files are optional for small direct changes.
- Define the public API and the contract callers need first. Choose verification and TDD based on the change and its risk; do not require extra tests for every simple getter.
- For work selected for TDD, use RED → implementation → GREEN against public behavior and contract. For C++ tests, use `cpp-automated-testing` and the relevant `cpp-test-*` skills.
- When reviewing `Test/RunTests.ps1` results, first read the run folder's `Summary.md`. If there are zero failures, stop there. Otherwise, read `Failures.md` in the same folder and investigate the cited files and lines. Read XML or individual logs only if `Failures.md` is missing or insufficient.

## Changes and Verification

- Check Git status before work and preserve unrelated changes.
- Do not inspect or modify trailing whitespace; leave it to editor settings.
- For documentation changes, use [VerifyDocs](Tools/Verify/VerifyDocs.cmd) to check links and conflict markers in the target scope. See the [usage guide](Tools/Verify/README.md) for arguments and scope.
- For ordinary implementation checks, use [VerifyProject](Tools/Verify/VerifyProject.cmd) to build the target project's `Debug_Editor|x64` configuration; MSBuild builds its declared project dependencies. Use [BuildSolution](Tools/Verify/BuildSolution.cmd) for all four x64 configurations when the task explicitly covers them or configuration-specific behavior needs verification.
- At completion, distinguish behavior that was verified from behavior and scope that remain unverified.

## Commit Messages

- Use `[English summary]Japanese comment`. Start the English summary with a work-type verb such as `Add`, `Update`, or `Refactor`, then briefly describe the change in Japanese (for example, `[Update]ビルド構成を追加`).
