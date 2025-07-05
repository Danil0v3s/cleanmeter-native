#pragma once

#include "../graphics_base.h"
#include <d3d10_1.h>

class DX10Graphics : public GraphicsBase {
public:
    DX10Graphics() = default;
    ~DX10Graphics() override = default;
    
    bool initialize() override {
        // TODO: Implement DX10 hooking
        return false;
    }
    
    void uninitialize() override {
        // TODO: Implement DX10 cleanup
    }
    
    void drawText(const char* text, int x, int y) override {
        // TODO: Implement DX10 text rendering
    }
}; 