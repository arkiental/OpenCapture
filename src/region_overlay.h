// Full-screen overlay that lets the user pick a rectangular region of a frozen
// screenshot. Behaviour is specified in docs/behavior/rectangular-region.md.
#pragma once

#include "surface.h"
#include "win.h"

namespace oc {

struct RegionCaptureResult {
    bool ok = false;     // false when the user cancelled
    RECT screenRect{};   // selected area in virtual-screen coordinates
    Image image;         // pixels of the selected area (from the frozen screen)
};

// Freezes the screen, runs the selection UI modally (own message loop) and
// returns the captured pixels. Safe to call from the UI thread only.
RegionCaptureResult CaptureRectangularRegion(HINSTANCE instance);

// True while CaptureRectangularRegion is running (used to ignore re-entrant
// hotkey presses).
bool IsRegionCaptureActive();

}  // namespace oc
