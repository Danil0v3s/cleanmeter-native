#pragma once

#include <Windows.h>

class GraphicsBase {
public:
    virtual ~GraphicsBase() = default;
    
    virtual bool initialize() = 0;
    virtual void uninitialize() = 0;
    virtual void drawText(const char* text, int x, int y) = 0;
    
protected:
    bool isInitialized_ = false;
}; 