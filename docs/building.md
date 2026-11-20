# Building

Requirements: Windows 10/11 x64, CMake ≥ 3.20 and **one** of the toolchains below.

## Option A: LLVM-MinGW (clang), no admin rights needed

```powershell
winget install --scope user MartinStorsjo.LLVM-MinGW.UCRT Kitware.CMake Ninja-build.Ninja
# open a new terminal so PATH is updated, then:
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_RC_COMPILER=llvm-windres
cmake --build build
ctest --test-dir build --output-on-failure
```

## Option B: Visual Studio / Build Tools (MSVC)

From a *Developer PowerShell*:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

This is what CI runs (`.github/workflows/build.yml`).

## Running

```powershell
build\OpenCapture.exe            # tray app; Ctrl+PrtSc captures a region
build\OpenCapture.exe --region   # capture a region right away
build\OpenCapture.exe --exit     # quit the running instance
```

If another program (for example another capture tool) already owns Ctrl+PrtSc,
OpenCapture shows a tray notification; use the tray icon or `--region` instead.

## CMake options

| Option | Default | Meaning |
|---|---|---|
| `OC_BUILD_TESTS` | `ON` | Build the unit tests and register them with CTest. |
