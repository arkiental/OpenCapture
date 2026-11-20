# OpenCapture

An open-source, lightweight screen capture tool for Windows, written in C++ and
the plain Win32 API. Its behaviour is modelled on FastStone Capture, re-created
from observation only (see the [clean-room policy](docs/clean-room.md)).

> **Status: early.** The first feature, **Rectangular Region capture**, is done.
> See the [roadmap](docs/roadmap.md) for what comes next.

## Rectangular Region capture

Press **Ctrl+PrtSc** (or click the tray icon). The screen freezes and you select an area:

* **Drag** with the left button, or **click** one corner and **click** the opposite corner.
* **Arrow keys** move the pointer by 1 px, **Enter/Space** fixes a corner or accepts,
  **Tab** swaps the fixed and the moving corner.
* A 5× **magnifier** with pointer coordinates and selection size sits in a screen
  corner and moves out of the way; **F1** hides it.
* **Esc**, right-click, middle-click or the mouse wheel cancels.

The capture opens in a result window where you can copy it (Ctrl+C) or save it as
PNG, JPEG or BMP (Ctrl+S). The exact rules are in the
[behaviour spec](docs/behavior/rectangular-region.md).

## Build

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_RC_COMPILER=llvm-windres
cmake --build build
ctest --test-dir build --output-on-failure
```

MSVC works too. Full instructions: [docs/building.md](docs/building.md).

## Usage

```
OpenCapture.exe            start the tray app (Ctrl+PrtSc = rectangular region)
OpenCapture.exe --region   capture a region now (starts the app if needed)
OpenCapture.exe --exit     quit the running instance
```

## Documentation

Everything is in [docs/](docs/README.md): architecture, building, behaviour specs
and the research logs they are based on.

## License

[MIT](LICENSE). OpenCapture is not affiliated with FastStone Corporation;
"FastStone Capture" is named only to describe the behaviour we compare against.
