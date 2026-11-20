# Clean-room policy

OpenCapture re-creates the *behaviour* of a commercial screen-capture tool
(FastStone Capture) as new, independently written open-source software. The
reference's licence forbids decompiling, disassembling or reverse-engineering
its binaries, so this project follows these rules:

## Allowed sources
* **Using the reference application normally** and observing what it does:
  screenshots of its windows, timing, pixel colours, window classes/styles that
  any program can query, responses to mouse and keyboard input.
* Its **user-facing documentation** (help file, website).
* Public Windows documentation and general knowledge.

## Not allowed
* Disassembling, decompiling, debugging or patching the reference binaries.
* Copying its code, images, icons, sounds, fonts, translations, data files or
  other assets into this repository.
* Copying its text verbatim (e.g. on-screen help). Write our own wording.
* Using its name or logo as OpenCapture's branding. Naming it to describe what
  we compare against is fine.
* Committing screenshots taken on a contributor's desktop (they leak private
  information and contain the reference's UI). Describe findings in text and
  numbers instead.

## How findings are recorded
1. Raw measurements go to `docs/research/<date>-<topic>.md` with the method used.
2. The resulting rules go to `docs/behavior/<feature>.md` (the spec).
3. Where possible, the rules become unit tests whose expected values are the
   measured numbers (see `tests/test_region_logic.cpp`).
