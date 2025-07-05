#include "hook_manager.h"
#include "../graphics/graphics_factory.h"
#include "../graphics/graphics_base.h"

// Timer callback for retry mechanism
static VOID CALLBACK RetryTimerCallback(PVOID lpParam, BOOLEAN TimerOrWaitFired) {
    HookManager* hookManager = static_cast<HookManager*>(lpParam);
    if (hookManager) {
        hookManager->onRetryTimer();
    }
}

HookManager::HookManager() : initialized_(false), retryTimer_(nullptr), needsRetry_(false) {
    InitializeCriticalSection(&criticalSection_);
}

HookManager::~HookManager() {
    uninitialize();
    DeleteCriticalSection(&criticalSection_);
}

bool HookManager::initialize() {
    EnterCriticalSection(&criticalSection_);
    
    if (initialized_) {
        LeaveCriticalSection(&criticalSection_);
        return true;
    }

    OutputDebugStringA("HookManager: Starting initialization\n");
    
    // Create graphics instance based on detected API
    graphics_ = GraphicsFactory::createGraphics();
    if (!graphics_) {
        // No graphics API detected yet - this is okay, we'll try again later
        // This allows the DLL to load into processes that don't have DirectX loaded yet
        OutputDebugStringA("HookManager: No graphics API detected, setting up retry mechanism\n");
        
        // Set up retry timer to check for graphics APIs periodically
        needsRetry_ = true;
        if (!CreateTimerQueueTimer(&retryTimer_, NULL, RetryTimerCallback, this, 2000, 2000, 0)) {
            OutputDebugStringA("HookManager: Failed to create retry timer\n");
        } else {
            OutputDebugStringA("HookManager: Retry timer created successfully\n");
        }
        
        initialized_ = true;  // Mark as initialized but without graphics hooks
        LeaveCriticalSection(&criticalSection_);
        return true;
    }

    OutputDebugStringA("HookManager: Graphics API detected, initializing hooks\n");
    
    // Initialize graphics hooks
    if (!graphics_->initialize()) {
        OutputDebugStringA("HookManager: Graphics initialization failed, setting up retry mechanism\n");
        
        // Set up retry timer to attempt initialization again
        needsRetry_ = true;
        if (!CreateTimerQueueTimer(&retryTimer_, NULL, RetryTimerCallback, this, 2000, 2000, 0)) {
            OutputDebugStringA("HookManager: Failed to create retry timer\n");
        } else {
            OutputDebugStringA("HookManager: Retry timer created successfully\n");
        }
        
        graphics_.reset();
        // Still mark as initialized to allow DLL loading
        initialized_ = true;
        LeaveCriticalSection(&criticalSection_);
        return true;
    }

    OutputDebugStringA("HookManager: Graphics hooks initialized successfully\n");
    initialized_ = true;
    LeaveCriticalSection(&criticalSection_);
    return true;
}

void HookManager::uninitialize() {
    EnterCriticalSection(&criticalSection_);
    
    // Stop retry timer
    if (retryTimer_) {
        DeleteTimerQueueTimer(NULL, retryTimer_, INVALID_HANDLE_VALUE);
        retryTimer_ = nullptr;
    }
    needsRetry_ = false;
    
    if (initialized_) {
        if (graphics_) {
            graphics_->uninitialize();
            graphics_.reset();
        }
        initialized_ = false;
    }
    
    LeaveCriticalSection(&criticalSection_);
}

void HookManager::onRetryTimer() {
    if (!needsRetry_) {
        return;
    }
    
    OutputDebugStringA("HookManager: Retry timer fired, attempting graphics initialization\n");
    
    if (tryInitializeGraphics()) {
        OutputDebugStringA("HookManager: Graphics initialization successful on retry\n");
        
        // Stop the retry timer since we succeeded
        EnterCriticalSection(&criticalSection_);
        if (retryTimer_) {
            DeleteTimerQueueTimer(NULL, retryTimer_, INVALID_HANDLE_VALUE);
            retryTimer_ = nullptr;
        }
        needsRetry_ = false;
        LeaveCriticalSection(&criticalSection_);
    } else {
        OutputDebugStringA("HookManager: Graphics initialization still failing, will retry again\n");
    }
}

bool HookManager::tryInitializeGraphics() {
    EnterCriticalSection(&criticalSection_);
    
    if (!initialized_ || graphics_) {
        bool result = graphics_ != nullptr;
        LeaveCriticalSection(&criticalSection_);
        return result;
    }

    OutputDebugStringA("HookManager: Attempting graphics initialization retry\n");
    
    // Try to create graphics instance again
    graphics_ = GraphicsFactory::createGraphics();
    if (!graphics_) {
        OutputDebugStringA("HookManager: No graphics API detected on retry\n");
        LeaveCriticalSection(&criticalSection_);
        return false;
    }

    OutputDebugStringA("HookManager: Graphics API detected on retry, initializing hooks\n");
    
    // Initialize graphics hooks
    if (!graphics_->initialize()) {
        OutputDebugStringA("HookManager: Graphics initialization failed on retry\n");
        graphics_.reset();
        LeaveCriticalSection(&criticalSection_);
        return false;
    }

    OutputDebugStringA("HookManager: Graphics hooks initialized successfully on retry\n");
    LeaveCriticalSection(&criticalSection_);
    return true;
} 