# Architecture

OpenCapture is a native Windows application in C++20 using only the Win32 API,
GDI and WIC: no frameworks, no runtime, no third-party libraries. The release
build is a single self-contained executable (about 0.6 MB).

## Process model
* **Single instance.** A named mutex guards the process. A second launch forwards
  its command line (`--region`, `--exit`) to the first through `WM_COPYDATA` and
  exits.
* **Tray application.** A hidden host window owns the tray icon, the global
  hotkey (`RegisterHotKey`, Ctrl+PrtSc) and the tray menu.
* **Per-monitor DPI aware (v2)** through the manifest; all overlay work is in
  physical pixels.

## Source layout

| File | Responsibility |
|---|---|
| `src/main.cpp` | Entry point, single instance, tray icon/menu, hotkey, command line. |
| `src/region_logic.*` | Pure, Win32-free rules: selection state machine, size-label placement, panel corner logic. Unit tested. |
| `src/region_overlay.*` | The selection overlay window: frozen screen, rendering, input. Runs a modal message loop and returns the captured pixels. |
| `src/surface.*` | `Image` (owning BGRA pixels) and `DibSurface` (DIB section + memory DC), screen grab, crop. |
| `src/image_io.*` | Save PNG/JPEG/BMP via WIC, copy to clipboard (`CF_DIB`). |
| `src/viewer.*` | Temporary result window (stand-in for the future editor). |
| `src/app_icon.*` | Icon drawn at runtime, so the repo has no binary assets. |
| `tests/` | Unit tests (no framework, run by CTest). |

## Overlay rendering
Two DIB surfaces of the virtual-screen size:

* `frozen_` – the screen grabbed once at the start (`BitBlt` with `CAPTUREBLT`).
* `back_` – what the window shows: `frozen_` plus the current overlay elements.

Each frame (`Overlay::Render`):
1. Restore every area drawn last frame from `frozen_` (lines are 1 px strips, so
   this is cheap).
2. XOR the crosshair or selection outline directly in the pixel memory.
3. Draw the magnifier panel: its zoom samples `back_` *after* the XOR lines but
   *before* the label, matching the reference.
4. Draw the size label.
5. `BitBlt` only the union of the old and new element areas to the window.

So a mouse move costs a few thin strips plus one 244 × 363 panel, independent of
screen size; there is no full-screen repaint per event.

The selection logic (`RegionSelector`) knows nothing about windows; the overlay
translates messages into calls (`LeftDown`, `Accept`, `SwapCorners`, …) and
reacts to the resulting state.

## Build
CMake (≥ 3.20). Works with llvm-mingw/clang (`-municode -static`) and with MSVC
(static CRT, manifest from `resources.rc`). See [building.md](building.md).
