#include "graphics_factory.h"
#include "dx9/dx9_graphics.h"
#include "dx10/dx10_graphics.h"
#include "dx11/dx11_graphics.h"
#include "dx12/dx12_graphics.h"
#include <delayimp.h>

// Exception handler for delay-loaded DLLs
LONG WINAPI DelayLoadExceptionHandler(PEXCEPTION_POINTERS pExceptionPointers) {
    if (pExceptionPointers->ExceptionRecord->ExceptionCode == VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND)) {
        // Handle missing DLL gracefully
        return EXCEPTION_EXECUTE_HANDLER;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

std::unique_ptr<GraphicsBase> GraphicsFactory::createGraphics() {
    GraphicsAPI api = detectGraphicsAPI();
    
    OutputDebugStringA("GraphicsFactory: Detected API: ");
    switch (api) {
    case GraphicsAPI::DirectX9:
        OutputDebugStringA("DirectX9\n");
        try {
            return std::make_unique<DX9Graphics>();
        } catch (...) {
            OutputDebugStringA("GraphicsFactory: Failed to create DX9Graphics (delay-load exception)\n");
            return nullptr;
        }
    case GraphicsAPI::DirectX10:
        OutputDebugStringA("DirectX10\n");
        try {
            return std::make_unique<DX10Graphics>();
        } catch (...) {
            OutputDebugStringA("GraphicsFactory: Failed to create DX10Graphics (delay-load exception)\n");
            return nullptr;
        }
    case GraphicsAPI::DirectX11:
        OutputDebugStringA("DirectX11\n");
        try {
            return std::make_unique<DX11Graphics>();
        } catch (...) {
            OutputDebugStringA("GraphicsFactory: Failed to create DX11Graphics (delay-load exception)\n");
            return nullptr;
        }
    case GraphicsAPI::DirectX12:
        OutputDebugStringA("DirectX12\n");
        try {
            return std::make_unique<DX12Graphics>();
        } catch (...) {
            OutputDebugStringA("GraphicsFactory: Failed to create DX12Graphics (delay-load exception)\n");
            return nullptr;
        }
    default:
        OutputDebugStringA("Unknown - No DirectX modules found\n");
        return nullptr;
    }
}

GraphicsAPI GraphicsFactory::detectGraphicsAPI() {
    // Check for DirectX modules in order of preference
    OutputDebugStringA("GraphicsFactory: Checking for DirectX modules...\n");
    
    // Use structured exception handling for delay-loaded DLLs
    __try {
        OutputDebugStringA("GraphicsFactory: Checking for d3d12.dll\n");
        if (isModuleLoaded("d3d12.dll")) {
            OutputDebugStringA("GraphicsFactory: Found d3d12.dll\n");
            return GraphicsAPI::DirectX12;
        }
        
        OutputDebugStringA("GraphicsFactory: Checking for d3d11.dll\n");
        if (isModuleLoaded("d3d11.dll")) {
            OutputDebugStringA("GraphicsFactory: Found d3d11.dll\n");
            return GraphicsAPI::DirectX11;
        }
        
        OutputDebugStringA("GraphicsFactory: Checking for d3d10.dll and d3d10_1.dll\n");
        if (isModuleLoaded("d3d10.dll") || isModuleLoaded("d3d10_1.dll")) {
            OutputDebugStringA("GraphicsFactory: Found d3d10.dll or d3d10_1.dll\n");
            return GraphicsAPI::DirectX10;
        }
        
        OutputDebugStringA("GraphicsFactory: Checking for d3d9.dll\n");
        if (isModuleLoaded("d3d9.dll")) {
            OutputDebugStringA("GraphicsFactory: Found d3d9.dll\n");
            return GraphicsAPI::DirectX9;
        }
    } __except (DelayLoadExceptionHandler(GetExceptionInformation())) {
        OutputDebugStringA("GraphicsFactory: Exception during DirectX module detection\n");
    }
    
    OutputDebugStringA("GraphicsFactory: No DirectX modules found in target process\n");
    return GraphicsAPI::Unknown;
}

bool GraphicsFactory::isModuleLoaded(const char* moduleName) {
    __try {
        HMODULE hModule = GetModuleHandleA(moduleName);
        return hModule != nullptr;
    } __except (DelayLoadExceptionHandler(GetExceptionInformation())) {
        // If we get an exception trying to check for the module, it's not loaded
        return false;
    }
} 