// Minimal capture window shown after a capture ("To Editor" output). It is a
// stand-in for the future editor: view at 100%, scroll, copy and save.
#pragma once

#include "surface.h"
#include "win.h"

namespace oc {

// Opens a new top-level viewer window that owns `img`.
HWND ShowCaptureViewer(HINSTANCE instance, Image img);

}  // namespace oc
