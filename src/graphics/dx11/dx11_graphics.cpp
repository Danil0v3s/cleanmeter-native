#include "dx11_graphics.h"
#include <detours.h>
#include <FW1FontWrapper.h>
#include <iostream>

DX11Graphics* DX11Graphics::instance_ = nullptr;

DX11Graphics::DX11Graphics() 
    : fontFactory_(nullptr), fontWrapper_(nullptr), fontInitialized_(false),
      originalPresent_(nullptr), originalResizeBuffers_(nullptr) {
    instance_ = this;
}

DX11Graphics::~DX11Graphics() {
    uninitialize();
    instance_ = nullptr;
}

bool DX11Graphics::initialize() {
    if (isInitialized_) {
        return true;
    }
    
    // Setup DirectX hooks
    if (!setupHooks()) {
        return false;
    }
    
    isInitialized_ = true;
    return true;
}

void DX11Graphics::uninitialize() {
    if (!isInitialized_) {
        return;
    }
    
    cleanupFont();
    removeHooks();
    
    renderTargetView_.reset();
    context_.reset();
    device_.reset();
    swapChain_.reset();
    
    isInitialized_ = false;
}

bool DX11Graphics::setupHooks() {
    OutputDebugStringA("DX11Graphics: Starting setupHooks\n");
    
    // Get DXGI module
    HMODULE dxgiModule = GetModuleHandleA("dxgi.dll");
    if (!dxgiModule) {
        OutputDebugStringA("DX11Graphics: Failed to get dxgi.dll module handle\n");
        return false;
    }
    
    OutputDebugStringA("DX11Graphics: dxgi.dll module found\n");
    
    // Instead of creating our own device, try to find an existing D3D11 device in the target process
    // This approach is safer and doesn't require creating graphics resources
    
    // Try to get the device from the target process's existing DirectX context
    // First, let's try a more lightweight approach - create a minimal device without swap chain
    ComPtr<ID3D11Device> tempDevice;
    ComPtr<ID3D11DeviceContext> tempContext;
    D3D_FEATURE_LEVEL featureLevel;
    
    OutputDebugStringA("DX11Graphics: Attempting to create minimal D3D11 device (no swap chain)\n");
    
    HRESULT hr = D3D11CreateDevice(
        nullptr,                    // Default adapter
        D3D_DRIVER_TYPE_HARDWARE,   // Hardware driver
        nullptr,                    // No software module
        0,                          // No flags
        nullptr,                    // Default feature levels
        0,                          // Default feature levels count
        D3D11_SDK_VERSION,          // SDK version
        tempDevice.getAddressOf(),  // Device
        &featureLevel,              // Feature level
        tempContext.getAddressOf()  // Context
    );
    
    if (FAILED(hr)) {
        char errorMsg[256];
        sprintf_s(errorMsg, "DX11Graphics: D3D11CreateDevice failed with HRESULT: 0x%08X\n", hr);
        OutputDebugStringA(errorMsg);
        
        // Try with WARP driver
        OutputDebugStringA("DX11Graphics: Retrying with WARP driver\n");
        hr = D3D11CreateDevice(
            nullptr,                    // Default adapter
            D3D_DRIVER_TYPE_WARP,       // WARP (software) driver
            nullptr,                    // No software module
            0,                          // No flags
            nullptr,                    // Default feature levels
            0,                          // Default feature levels count
            D3D11_SDK_VERSION,          // SDK version
            tempDevice.getAddressOf(),  // Device
            &featureLevel,              // Feature level
            tempContext.getAddressOf()  // Context
        );
        
        if (FAILED(hr)) {
            sprintf_s(errorMsg, "DX11Graphics: D3D11CreateDevice with WARP also failed: 0x%08X\n", hr);
            OutputDebugStringA(errorMsg);
            
            // Try reference driver as last resort
            OutputDebugStringA("DX11Graphics: Retrying with reference driver\n");
            hr = D3D11CreateDevice(
                nullptr,                    // Default adapter
                D3D_DRIVER_TYPE_REFERENCE,  // Reference driver
                nullptr,                    // No software module
                0,                          // No flags
                nullptr,                    // Default feature levels
                0,                          // Default feature levels count
                D3D11_SDK_VERSION,          // SDK version
                tempDevice.getAddressOf(),  // Device
                &featureLevel,              // Feature level
                tempContext.getAddressOf()  // Context
            );
            
            if (FAILED(hr)) {
                sprintf_s(errorMsg, "DX11Graphics: All D3D11CreateDevice attempts failed. Last error: 0x%08X\n", hr);
                OutputDebugStringA(errorMsg);
                return false;
            }
            
            OutputDebugStringA("DX11Graphics: Successfully created device with reference driver\n");
        } else {
            OutputDebugStringA("DX11Graphics: Successfully created device with WARP driver\n");
        }
    } else {
        OutputDebugStringA("DX11Graphics: Successfully created device with hardware driver\n");
    }
    
    // Now we need to get the swap chain vtable. Since we can't create our own swap chain,
    // we'll try to hook into the existing swap chain in the target process
    
    // Alternative approach: Try to find the swap chain interface from loaded modules
    OutputDebugStringA("DX11Graphics: Looking for existing swap chain in target process\n");
    
    // Get the DXGI factory to create a minimal swap chain for vtable extraction
    ComPtr<IDXGIFactory> factory;
    hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)factory.getAddressOf());
    if (FAILED(hr)) {
        OutputDebugStringA("DX11Graphics: Failed to create DXGI factory\n");
        return false;
    }
    
    // Get the adapter
    ComPtr<IDXGIAdapter> adapter;
    hr = factory->EnumAdapters(0, adapter.getAddressOf());
    if (FAILED(hr)) {
        OutputDebugStringA("DX11Graphics: Failed to enumerate adapters\n");
        return false;
    }
    
    // Find a valid window handle for the swap chain
    HWND targetWindow = nullptr;
    
    // Try to find the main window of the current process
    struct WindowSearchData {
        DWORD processId;
        HWND foundWindow;
    };
    
    WindowSearchData searchData = { GetCurrentProcessId(), nullptr };
    
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        WindowSearchData* data = reinterpret_cast<WindowSearchData*>(lParam);
        DWORD windowProcessId;
        GetWindowThreadProcessId(hwnd, &windowProcessId);
        
        if (windowProcessId == data->processId && IsWindowVisible(hwnd)) {
            // Check if this is a main window (has a title)
            char windowTitle[256];
            if (GetWindowTextA(hwnd, windowTitle, sizeof(windowTitle)) > 0) {
                data->foundWindow = hwnd;
                return FALSE; // Stop enumeration
            }
        }
        return TRUE; // Continue enumeration
    }, reinterpret_cast<LPARAM>(&searchData));
    
    targetWindow = searchData.foundWindow;
    
    if (!targetWindow) {
        OutputDebugStringA("DX11Graphics: No main window found, trying desktop window\n");
        targetWindow = GetDesktopWindow();
    }
    
    if (!targetWindow || !IsWindow(targetWindow)) {
        OutputDebugStringA("DX11Graphics: No valid window handle found for swap chain creation\n");
        return false;
    }
    
    char windowMsg[256];
    sprintf_s(windowMsg, "DX11Graphics: Using window handle: 0x%p\n", targetWindow);
    OutputDebugStringA(windowMsg);
    
    // Create a minimal swap chain descriptor
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = 1;
    swapChainDesc.BufferDesc.Width = 1;
    swapChainDesc.BufferDesc.Height = 1;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = targetWindow;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = TRUE;
    
    ComPtr<IDXGISwapChain> tempSwapChain;
    OutputDebugStringA("DX11Graphics: Creating minimal swap chain for vtable extraction\n");
    
    hr = factory->CreateSwapChain(tempDevice.get(), &swapChainDesc, tempSwapChain.getAddressOf());
    if (FAILED(hr)) {
        char errorMsg[256];
        sprintf_s(errorMsg, "DX11Graphics: CreateSwapChain failed: 0x%08X\n", hr);
        OutputDebugStringA(errorMsg);
        
        // If we can't create a swap chain, we can't get the vtable for hooking
        // This means we can't hook DirectX in this process
        OutputDebugStringA("DX11Graphics: Cannot create swap chain for vtable extraction - DirectX hooking not possible\n");
        return false;
    }
    
    OutputDebugStringA("DX11Graphics: Successfully created swap chain for vtable extraction\n");
    
    // Get vtable addresses
    void** swapChainVTable = *reinterpret_cast<void***>(tempSwapChain.get());
    void* presentAddr = swapChainVTable[8];  // Present is at index 8
    void* resizeBuffersAddr = swapChainVTable[13]; // ResizeBuffers is at index 13
    
    OutputDebugStringA("DX11Graphics: Got vtable addresses, setting up hooks\n");
    
    // Hook Present
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    originalPresent_ = presentAddr;
    DetourAttach(&originalPresent_, presentHook);
    
    // Hook ResizeBuffers
    originalResizeBuffers_ = resizeBuffersAddr;
    DetourAttach(&originalResizeBuffers_, resizeBuffersHook);
    
    LONG result = DetourTransactionCommit();
    if (result != NO_ERROR) {
        char errorMsg[256];
        sprintf_s(errorMsg, "DX11Graphics: DetourTransactionCommit failed with error: %ld\n", result);
        OutputDebugStringA(errorMsg);
        return false;
    }
    
    OutputDebugStringA("DX11Graphics: Hooks installed successfully\n");
    return true;
}

void DX11Graphics::removeHooks() {
    if (originalPresent_ || originalResizeBuffers_) {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        
        if (originalPresent_) {
            DetourDetach(&originalPresent_, presentHook);
        }
        if (originalResizeBuffers_) {
            DetourDetach(&originalResizeBuffers_, resizeBuffersHook);
        }
        
        DetourTransactionCommit();
        
        originalPresent_ = nullptr;
        originalResizeBuffers_ = nullptr;
    }
}

bool DX11Graphics::initializeFont() {
    if (fontInitialized_ || !device_) {
        return fontInitialized_;
    }
    
    // Create FW1 factory
    HRESULT hr = FW1CreateFactory(FW1_VERSION, &fontFactory_);
    if (FAILED(hr)) {
        return false;
    }
    
    // Create font wrapper
    hr = fontFactory_->CreateFontWrapper(device_.get(), L"Arial", &fontWrapper_);
    if (FAILED(hr)) {
        fontFactory_->Release();
        fontFactory_ = nullptr;
        return false;
    }
    
    fontInitialized_ = true;
    return true;
}

void DX11Graphics::cleanupFont() {
    if (fontWrapper_) {
        fontWrapper_->Release();
        fontWrapper_ = nullptr;
    }
    if (fontFactory_) {
        fontFactory_->Release();
        fontFactory_ = nullptr;
    }
    fontInitialized_ = false;
}

void DX11Graphics::saveState() {
    if (!context_) return;
    
    // Save viewport
    savedState_.viewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    context_->RSGetViewports(&savedState_.viewportCount, savedState_.viewports);
    
    // Save render targets
    context_->OMGetRenderTargets(1, savedState_.renderTargetView.getAddressOf(), 
                                savedState_.depthStencilView.getAddressOf());
}

void DX11Graphics::restoreState() {
    if (!context_) return;
    
    // Restore viewport
    context_->RSSetViewports(savedState_.viewportCount, savedState_.viewports);
    
    // Restore render targets
    context_->OMSetRenderTargets(1, savedState_.renderTargetView.getAddressOf(), 
                                savedState_.depthStencilView.get());
}

void DX11Graphics::drawText(const char* text, int x, int y) {
    if (!fontInitialized_ || !fontWrapper_ || !context_) {
        return;
    }
    
    // Convert to wide string
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    std::unique_ptr<wchar_t[]> wideText(new wchar_t[wideLen]);
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wideText.get(), wideLen);
    
    // Draw text
    fontWrapper_->DrawString(
        context_.get(),
        wideText.get(),
        16.0f,  // Font size
        static_cast<float>(x),
        static_cast<float>(y),
        0xFFFFFFFF,  // White color
        FW1_RESTORESTATE
    );
}

void DX11Graphics::onPresent(IDXGISwapChain* swapChain) {
    if (!swapChain) return;
    
    // Get device if we don't have it
    if (!device_) {
        swapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(device_.getAddressOf()));
        if (device_) {
            device_->GetImmediateContext(context_.getAddressOf());
        }
    }
    
    // Get render target view if we don't have it
    if (!renderTargetView_ && device_) {
        ComPtr<ID3D11Texture2D> backBuffer;
        swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.getAddressOf()));
        if (backBuffer) {
            device_->CreateRenderTargetView(backBuffer.get(), nullptr, renderTargetView_.getAddressOf());
        }
    }
    
    // Initialize font if needed
    if (!fontInitialized_) {
        initializeFont();
    }
    
    // Save current state
    saveState();
    
    // Set our render target
    if (renderTargetView_) {
        context_->OMSetRenderTargets(1, renderTargetView_.getAddressOf(), nullptr);
    }
    
    // Draw "Hello World" text
    drawText("Hello World", 10, 10);
    
    // Restore state
    restoreState();
}

void DX11Graphics::onResizeBuffers(IDXGISwapChain* swapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags) {
    // Release render target view before resize
    renderTargetView_.reset();
}

HRESULT STDMETHODCALLTYPE DX11Graphics::presentHook(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags) {
    if (instance_) {
        instance_->onPresent(swapChain);
    }
    
    // Call original Present
    using PresentFunc = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
    PresentFunc originalPresent = reinterpret_cast<PresentFunc>(instance_->originalPresent_);
    return originalPresent(swapChain, syncInterval, flags);
}

HRESULT STDMETHODCALLTYPE DX11Graphics::resizeBuffersHook(IDXGISwapChain* swapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags) {
    if (instance_) {
        instance_->onResizeBuffers(swapChain, bufferCount, width, height, newFormat, swapChainFlags);
    }
    
    // Call original ResizeBuffers
    using ResizeBuffersFunc = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
    ResizeBuffersFunc originalResizeBuffers = reinterpret_cast<ResizeBuffersFunc>(instance_->originalResizeBuffers_);
    return originalResizeBuffers(swapChain, bufferCount, width, height, newFormat, swapChainFlags);
} 