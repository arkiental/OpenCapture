#include "surface.h"

#include <cstring>

namespace oc {

bool DibSurface::Create(int width, int height) {
    Destroy();
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height;  // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(nullptr);
    dc_ = CreateCompatibleDC(screen);
    ReleaseDC(nullptr, screen);
    if (!dc_) return false;

    void* bits = nullptr;
    bmp_ = CreateDIBSection(dc_, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp_) {
        DeleteDC(dc_);
        dc_ = nullptr;
        return false;
    }
    old_ = SelectObject(dc_, bmp_);
    bits_ = static_cast<uint32_t*>(bits);
    width_ = width;
    height_ = height;
    return true;
}

void DibSurface::Destroy() {
    if (dc_) {
        if (old_) SelectObject(dc_, old_);
        DeleteDC(dc_);
    }
    if (bmp_) DeleteObject(bmp_);
    dc_ = nullptr;
    bmp_ = nullptr;
    old_ = nullptr;
    bits_ = nullptr;
    width_ = height_ = 0;
}

RECT VirtualScreenRect() {
    RECT r;
    r.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    r.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    r.right = r.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    r.bottom = r.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    return r;
}

bool GrabScreen(const RECT& area, DibSurface& out) {
    const int w = area.right - area.left;
    const int h = area.bottom - area.top;
    if (out.Width() != w || out.Height() != h) {
        if (!out.Create(w, h)) return false;
    }
    HDC screen = GetDC(nullptr);
    // CAPTUREBLT includes layered (translucent / tool) windows in the grab.
    const BOOL ok = BitBlt(out.Dc(), 0, 0, w, h, screen, area.left, area.top, SRCCOPY | CAPTUREBLT);
    ReleaseDC(nullptr, screen);
    GdiFlush();
    return ok != FALSE;
}

Image CropToImage(const DibSurface& src, int x, int y, int w, int h) {
    Image img;
    if (w <= 0 || h <= 0) return img;
    img.width = w;
    img.height = h;
    img.pixels.resize(static_cast<size_t>(w) * h);
    for (int row = 0; row < h; ++row) {
        const uint32_t* s = src.Row(y + row) + x;
        uint32_t* d = img.pixels.data() + static_cast<size_t>(row) * w;
        for (int col = 0; col < w; ++col) d[col] = s[col] | 0xFF000000u;
    }
    return img;
}

}  // namespace oc
