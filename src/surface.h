// 32-bit pixel buffers: an owning Image and a GDI-selectable DIB surface.
#pragma once

#include <cstdint>
#include <vector>

#include "win.h"

namespace oc {

// Plain top-down BGRA image (alpha is always 255 for screen captures).
struct Image {
    int width = 0;
    int height = 0;
    std::vector<uint32_t> pixels;  // 0x00RRGGBB layout in memory as B,G,R,A

    bool Empty() const { return width <= 0 || height <= 0; }
};

// A top-down 32bpp DIB section selected into its own memory DC, so it can be
// both drawn with GDI and touched directly through `bits`.
class DibSurface {
public:
    DibSurface() = default;
    ~DibSurface() { Destroy(); }
    DibSurface(const DibSurface&) = delete;
    DibSurface& operator=(const DibSurface&) = delete;

    bool Create(int width, int height);
    void Destroy();

    HDC Dc() const { return dc_; }
    uint32_t* Bits() const { return bits_; }
    int Width() const { return width_; }
    int Height() const { return height_; }
    uint32_t* Row(int y) const { return bits_ + static_cast<size_t>(y) * width_; }

private:
    HDC dc_ = nullptr;
    HBITMAP bmp_ = nullptr;
    HGDIOBJ old_ = nullptr;
    uint32_t* bits_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

// Virtual-screen rectangle (all monitors) in physical pixels.
RECT VirtualScreenRect();

// Copies the whole virtual screen into `out` (resized as needed).
bool GrabScreen(const RECT& area, DibSurface& out);

// Copies an area of a surface into an owning Image with opaque alpha.
Image CropToImage(const DibSurface& src, int x, int y, int w, int h);

}  // namespace oc
