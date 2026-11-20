// Saving captures to disk (PNG/JPEG/BMP via WIC) and copying to the clipboard.
#pragma once

#include <string>

#include "surface.h"
#include "win.h"

namespace oc {

enum class ImageFormat { Png, Jpeg, Bmp };

// Picks the format from the file extension (defaults to PNG).
ImageFormat FormatFromPath(const std::wstring& path);

// Writes `img` to `path`. Returns false (and fills *error) on failure.
bool SaveImage(const Image& img, const std::wstring& path, ImageFormat fmt, int jpegQuality = 90,
               std::wstring* error = nullptr);

// Places the image on the clipboard as CF_DIB.
bool CopyImageToClipboard(HWND owner, const Image& img);

}  // namespace oc
