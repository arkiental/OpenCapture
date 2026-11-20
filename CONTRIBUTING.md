# Contributing

## Workflow for a new feature

1. **Observe** the reference application (black-box only, see
   [docs/clean-room.md](docs/clean-room.md)) and write the raw measurements to
   `docs/research/<YYYY-MM-DD>-<feature>.md`.
2. **Specify** the behaviour in `docs/behavior/<feature>.md`.
3. **Test** the rules: put pure logic in a Win32-free module and encode the
   measured numbers as unit tests under `tests/`.
4. **Implement**, then verify against the reference with the same scenarios.
5. **Document**: update `docs/roadmap.md`, `docs/README.md` (index) and
   `CHANGELOG.md` in the same commit/PR as the code.

Docs are part of the change, not a follow-up: a feature is done when its spec,
tests and changelog entry are in the repository.

## Code style

* C++20, Win32 API, no third-party dependencies unless discussed first.
* `PascalCase` functions and types, `camelCase` locals, `trailing_` members,
  `kConstants`, everything in `namespace oc`.
* Keep files small and focused; keep logic testable without a desktop.
* Must build warning-free with clang (llvm-mingw) and MSVC.

## Commits

Small, focused commits with an imperative subject line, e.g.
`Add fixed-region capture spec`.
