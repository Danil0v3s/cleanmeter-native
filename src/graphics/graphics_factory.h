#pragma once

#include <memory>
#include "graphics_base.h"

enum class GraphicsAPI {
    Unknown,
    DirectX9,
    DirectX10,
    DirectX11,
    DirectX12,
    OpenGL,
    Vulkan
};

class GraphicsFactory {
public:
    static std::unique_ptr<GraphicsBase> createGraphics();
    static GraphicsAPI detectGraphicsAPI();
    
private:
    static bool isModuleLoaded(const char* moduleName);
}; 