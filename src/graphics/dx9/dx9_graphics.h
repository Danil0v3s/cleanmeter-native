#pragma once

#include "../graphics_base.h"
#include <d3d9.h>

class DX9Graphics : public GraphicsBase {
public:
    DX9Graphics() = default;
    ~DX9Graphics() override = default;
    
    bool initialize() override {
        // TODO: Implement DX9 hooking
        return false;
    }
    
    void uninitialize() override {
        // TODO: Implement DX9 cleanup
    }
    
    void drawText(const char* text, int x, int y) override {
        // TODO: Implement DX9 text rendering
    }
}; 