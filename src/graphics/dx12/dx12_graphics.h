#pragma once

#include "../graphics_base.h"
#include <d3d12.h>

class DX12Graphics : public GraphicsBase {
public:
    DX12Graphics() = default;
    ~DX12Graphics() override = default;
    
    bool initialize() override {
        // TODO: Implement DX12 hooking
        return false;
    }
    
    void uninitialize() override {
        // TODO: Implement DX12 cleanup
    }
    
    void drawText(const char* text, int x, int y) override {
        // TODO: Implement DX12 text rendering
    }
}; 