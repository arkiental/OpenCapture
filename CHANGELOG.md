# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/).

## [0.1.0] - 2026-11-20

### Added
- Rectangular Region capture with the reference's interaction model: frozen
  screen, hidden pointer, full-screen XOR crosshair, inclusive selection,
  drag or click-move-click selection (4 px click threshold), Enter/Space,
  1 px arrow-key movement, Tab corner swap, Esc/right/middle/wheel cancel.
- Magnifier panel (5× zoom, coordinates and size, help) that jumps between the
  bottom-right and top-left monitor corners with a 90 px margin; F1 toggles it.
- Size label with the measured direction, offset and edge-flip rules.
- Result window: view, scroll, copy to clipboard, save as PNG/JPEG/BMP.
- Tray application, Ctrl+PrtSc global hotkey, single instance,
  `--region` / `--exit` command line.
- Unit tests encoding the measured behaviour; CMake build for clang and MSVC; CI.
- Documentation: architecture, building, clean-room policy, behaviour spec,
  research log, roadmap.
