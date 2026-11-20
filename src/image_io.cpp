#include "image_io.h"

#include <objbase.h>
#include <wincodec.h>

#include <cstring>
#include <cwctype>

namespace oc {
namespace {

template <typename T>
struct ComPtr {
    T* p = nullptr;
    ~ComPtr() {
        if (p) p->Release();
    }
    T** operator&() { return &p; }
    T* operator->() const { return p; }
    explicit operator bool() const { return p != nullptr; }
};

std::wstring Lower(std::wstring s) {
    for (auto& c : s) c = static_cast<wchar_t>(std::towlower(c));
    return s;
}

}  // namespace

ImageFormat FormatFromPath(const std::wstring& path) {
    const auto dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos) return ImageFormat::Png;
    const std::wstring ext = Lower(path.substr(dot + 1));
    if (ext == L"jpg" || ext == L"jpeg" || ext == L"jpe") return ImageFormat::Jpeg;
    if (ext == L"bmp") return ImageFormat::Bmp;
    return ImageFormat::Png;
}

bool SaveImage(const Image& img, const std::wstring& path, ImageFormat fmt, int jpegQuality, std::wstring* error) {
    auto fail = [&](const wchar_t* what, HRESULT hr) {
        if (error) {
            wchar_t buf[160];
            swprintf(buf, 160, L"%ls failed (0x%08lX)", what, static_cast<unsigned long>(hr));
            *error = buf;
        }
        return false;
    };
    if (img.Empty()) return fail(L"Empty image", E_INVALIDARG);

    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&factory.p));
    if (FAILED(hr)) return fail(L"Creating the WIC factory", hr);

    ComPtr<IWICBitmap> bitmap;
    hr = factory->CreateBitmapFromMemory(img.width, img.height, GUID_WICPixelFormat32bppBGRA, img.width * 4,
                                         static_cast<UINT>(img.pixels.size() * 4),
                                         reinterpret_cast<BYTE*>(const_cast<uint32_t*>(img.pixels.data())),
                                         &bitmap.p);
    if (FAILED(hr)) return fail(L"Wrapping the pixels", hr);

    ComPtr<IWICStream> stream;
    hr = factory->CreateStream(&stream.p);
    if (SUCCEEDED(hr)) hr = stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE);
    if (FAILED(hr)) return fail(L"Opening the file", hr);

    const GUID container = fmt == ImageFormat::Jpeg  ? GUID_ContainerFormatJpeg
                           : fmt == ImageFormat::Bmp ? GUID_ContainerFormatBmp
                                                     : GUID_ContainerFormatPng;
    ComPtr<IWICBitmapEncoder> encoder;
    hr = factory->CreateEncoder(container, nullptr, &encoder.p);
    if (SUCCEEDED(hr)) hr = encoder->Initialize(stream.p, WICBitmapEncoderNoCache);
    if (FAILED(hr)) return fail(L"Creating the encoder", hr);

    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> props;
    hr = encoder->CreateNewFrame(&frame.p, &props.p);
    if (FAILED(hr)) return fail(L"Creating the frame", hr);
    if (fmt == ImageFormat::Jpeg && props) {
        PROPBAG2 opt{};
        opt.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
        VARIANT v;
        VariantInit(&v);
        v.vt = VT_R4;
        v.fltVal = static_cast<float>(jpegQuality) / 100.0f;
        props->Write(1, &opt, &v);
    }
    hr = frame->Initialize(props.p);
    if (SUCCEEDED(hr)) hr = frame->SetSize(img.width, img.height);
    // Screen captures are opaque: store 24-bit colour, WriteSource converts.
    WICPixelFormatGUID pf = GUID_WICPixelFormat24bppBGR;
    if (SUCCEEDED(hr)) hr = frame->SetPixelFormat(&pf);
    if (SUCCEEDED(hr)) hr = frame->WriteSource(bitmap.p, nullptr);
    if (SUCCEEDED(hr)) hr = frame->Commit();
    if (SUCCEEDED(hr)) hr = encoder->Commit();
    if (FAILED(hr)) return fail(L"Encoding the image", hr);
    return true;
}

bool CopyImageToClipboard(HWND owner, const Image& img) {
    if (img.Empty()) return false;
    const size_t pixelBytes = img.pixels.size() * 4;
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + pixelBytes);
    if (!mem) return false;
    auto* bih = static_cast<BITMAPINFOHEADER*>(GlobalLock(mem));
    std::memset(bih, 0, sizeof(*bih));
    bih->biSize = sizeof(BITMAPINFOHEADER);
    bih->biWidth = img.width;
    bih->biHeight = img.height;  // bottom-up, the most widely understood layout
    bih->biPlanes = 1;
    bih->biBitCount = 32;
    bih->biCompression = BI_RGB;
    bih->biSizeImage = static_cast<DWORD>(pixelBytes);
    auto* dst = reinterpret_cast<uint8_t*>(bih + 1);
    const size_t stride = static_cast<size_t>(img.width) * 4;
    for (int y = 0; y < img.height; ++y)
        std::memcpy(dst + stride * (img.height - 1 - y), img.pixels.data() + static_cast<size_t>(y) * img.width,
                    stride);
    GlobalUnlock(mem);

    if (!OpenClipboard(owner)) {
        GlobalFree(mem);
        return false;
    }
    EmptyClipboard();
    const bool ok = SetClipboardData(CF_DIB, mem) != nullptr;
    CloseClipboard();
    if (!ok) GlobalFree(mem);
    return ok;
}

}  // namespace oc
