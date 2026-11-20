// The application icon is drawn at runtime (no binary assets in the repo).
#pragma once

#include "win.h"

namespace oc {

// Returns a new icon of `size` x `size` pixels; caller owns it (DestroyIcon).
HICON CreateAppIcon(int size);

}  // namespace oc
