#pragma once

#include "../graphics_base.h"
#include <d3d11.h>
#include <dxgi.h>
#include <memory>

// Forward declarations
struct IFW1Factory;
struct IFW1FontWrapper;

template<typename T>
class ComPtr {
private:
    T* ptr_;
    
public:
    ComPtr() : ptr_(nullptr) {}
    ~ComPtr() { if (ptr_) ptr_->Release(); }
    
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    
    ComPtr(ComPtr&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }
    
    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) {
            if (ptr_) ptr_->Release();
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }
    
    T* get() const { return ptr_; }
    T** getAddressOf() { return &ptr_; }
    T* operator->() const { return ptr_; }
    operator bool() const { return ptr_ != nullptr; }
    
    void reset() {
        if (ptr_) {
            ptr_->Release();
            ptr_ = nullptr;
        }
    }
};

class DX11Graphics : public GraphicsBase {
private:
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGISwapChain> swapChain_;
    ComPtr<ID3D11RenderTargetView> renderTargetView_;
    
    // Font rendering
    IFW1Factory* fontFactory_;
    IFW1FontWrapper* fontWrapper_;
    bool fontInitialized_;
    
    // Hook data
    void* originalPresent_;
    void* originalResizeBuffers_;
    
    // State management
    struct D3D11State {
        UINT viewportCount;
        D3D11_VIEWPORT viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
        ComPtr<ID3D11RenderTargetView> renderTargetView;
        ComPtr<ID3D11DepthStencilView> depthStencilView;
    } savedState_;
    
    bool initializeFont();
    void cleanupFont();
    
    void saveState();
    void restoreState();
    
    // Hook functions
    static HRESULT STDMETHODCALLTYPE presentHook(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags);
    static HRESULT STDMETHODCALLTYPE resizeBuffersHook(IDXGISwapChain* swapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags);
    
    bool setupHooks();
    void removeHooks();
    
public:
    DX11Graphics();
    ~DX11Graphics() override;
    
    bool initialize() override;
    void uninitialize() override;
    void drawText(const char* text, int x, int y) override;
    
    void onPresent(IDXGISwapChain* swapChain);
    void onResizeBuffers(IDXGISwapChain* swapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags);
    
    static DX11Graphics* instance_;
}; 