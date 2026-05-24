#include "WinThumbnailExtractor.h"

#ifdef Q_OS_WIN

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <shobjidl.h>
#include <objbase.h>
#include <comdef.h>
#include <thumbcache.h>
#include <initguid.h>
#include <QDir>

#ifndef BHID_ThumbnailHandler
DEFINE_GUID(BHID_ThumbnailHandler, 0x7b2e650a, 0x8e20, 0x4f4a, 0xb0, 0x9e, 0x65, 0x97, 0xaf, 0xc7, 0x2e, 0xc0);
#endif

#include <shobjidl_core.h>

QImage WinThumbnailExtractor::extract(const QString &path, const QSize &requestedSize)
{
    QImage result;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool needUninit = (hr == S_OK || hr == S_FALSE);

    IShellItem *pItem = nullptr;
    std::wstring wpath = QDir::toNativeSeparators(path).toStdWString();
    hr = SHCreateItemFromParsingName(wpath.c_str(), nullptr, IID_PPV_ARGS(&pItem));
    if (SUCCEEDED(hr)) {
        IShellItemImageFactory *pFactory = nullptr;
        hr = pItem->QueryInterface(IID_PPV_ARGS(&pFactory));
        if (SUCCEEDED(hr)) {
            HBITMAP hBmp = nullptr;
            UINT cx = qMax(requestedSize.width(), requestedSize.height());
            SIZE sz = { (LONG)cx, (LONG)cx };
            
            // Try thumbnail-only first
            hr = pFactory->GetImage(sz, SIIGBF_THUMBNAILONLY, &hBmp);
            if (FAILED(hr)) {
                hr = pFactory->GetImage(sz, SIIGBF_RESIZETOFIT, &hBmp);
            }
            
            if (SUCCEEDED(hr) && hBmp) {
                BITMAP bmp;
                if (GetObject(hBmp, sizeof(BITMAP), &bmp)) {
                    QImage img(bmp.bmWidth, bmp.bmHeight, QImage::Format_ARGB32_Premultiplied);
                    HDC hdc = GetDC(nullptr);
                    BITMAPINFO bmi = {};
                    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                    bmi.bmiHeader.biWidth = bmp.bmWidth;
                    bmi.bmiHeader.biHeight = -bmp.bmHeight;
                    bmi.bmiHeader.biPlanes = 1;
                    bmi.bmiHeader.biBitCount = 32;
                    bmi.bmiHeader.biCompression = BI_RGB;
                    
                    GetDIBits(hdc, hBmp, 0, bmp.bmHeight, img.bits(), &bmi, DIB_RGB_COLORS);
                    ReleaseDC(nullptr, hdc);
                    result = img;
                }
                DeleteObject(hBmp);
            }
            pFactory->Release();
        }
        pItem->Release();
    }

    if (needUninit) {
        CoUninitialize();
    }

    if (!result.isNull() && result.hasAlphaChannel()) {
        result = result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }
    
    return result;
}

#endif
