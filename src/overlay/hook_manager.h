#pragma once

#include <Windows.h>
#include <memory>

class GraphicsBase;

class HookManager {
private:
    std::unique_ptr<GraphicsBase> graphics_;
    bool initialized_;
    CRITICAL_SECTION criticalSection_;  // Thread safety
    HANDLE retryTimer_;  // Timer for retrying graphics initialization
    bool needsRetry_;    // Flag to indicate if retry is needed

public:
    HookManager();
    ~HookManager();

    bool initialize();
    void uninitialize();
    bool tryInitializeGraphics();  // Retry graphics initialization
    void onRetryTimer();           // Called when retry timer fires
    
    bool isInitialized() const { return initialized_; }
    bool hasGraphicsHooks() const { return graphics_ != nullptr; }
}; 