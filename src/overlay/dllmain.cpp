#include <Windows.h>
#include "hook_manager.h"
#include <fstream>
#include <string>

HMODULE g_hModule = nullptr;
HookManager* g_hookManager = nullptr;

void WriteLogFile(const char* message) {
    // Create temp directory if it doesn't exist
    // CreateDirectoryA("C:\\temp", NULL);
    
    std::ofstream logFile("C:\\Users\\danil\\Desktop\\overlay_debug.log", std::ios::app);
    if (logFile.is_open()) {
        logFile << message << std::endl;
        logFile.close();
    }
}

void DebugOut(const char* message) {
    // Try multiple output methods
    OutputDebugStringA(message);
    WriteLogFile(message);
}

// Function to get the directory where this DLL is located
std::wstring GetDllDirectory() {
    wchar_t dllPath[MAX_PATH];
    if (GetModuleFileNameW(g_hModule, dllPath, MAX_PATH) == 0) {
        return L"";
    }
    
    std::wstring dllDir(dllPath);
    size_t lastSlash = dllDir.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        dllDir = dllDir.substr(0, lastSlash);
    }
    
    return dllDir;
}

// Function to preload dependencies from the correct location
bool PreloadDependencies() {
    std::wstring dllDir = GetDllDirectory();
    if (dllDir.empty()) {
        DebugOut("OverlayHook: Failed to get DLL directory");
        return false;
    }
    
    // Construct path to FW1FontWrapper.x64.dll
    std::wstring fw1Path = dllDir + L"\\FW1FontWrapper.x64.dll";
    
    DebugOut("OverlayHook: Attempting to load FW1FontWrapper from: ");
    WriteLogFile(std::string(fw1Path.begin(), fw1Path.end()).c_str());
    
    // Try to load FW1FontWrapper from the DLL directory
    HMODULE hFW1 = LoadLibraryW(fw1Path.c_str());
    if (!hFW1) {
        DWORD error = GetLastError();
        DebugOut("OverlayHook: Failed to load FW1FontWrapper.x64.dll from DLL directory");
        
        char errorMsg[256];
        sprintf_s(errorMsg, "Error code: %lu", error);
        DebugOut(errorMsg);
        
        // Try loading from system PATH as fallback
        hFW1 = LoadLibraryA("FW1FontWrapper.x64.dll");
        if (!hFW1) {
            DebugOut("OverlayHook: FW1FontWrapper.x64.dll not found in system PATH either");
            return false;
        } else {
            DebugOut("OverlayHook: FW1FontWrapper.x64.dll loaded from system PATH");
        }
    } else {
        DebugOut("OverlayHook: FW1FontWrapper.x64.dll loaded successfully from DLL directory");
    }
    
    // Don't free the library here - we want it to stay loaded
    return true;
}

// Global flag to track initialization state
bool g_initializationFailed = false;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        {
            DebugOut("OverlayHook: DLL_PROCESS_ATTACH called - Starting initialization");
            
            DisableThreadLibraryCalls(hModule);
            g_hModule = hModule;
            
            // Check for basic dependencies first
            HMODULE hUser32 = GetModuleHandleA("user32.dll");
            HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
            
            if (!hUser32 || !hKernel32) {
                DebugOut("OverlayHook: ERROR - Basic Windows DLLs not found");
                return FALSE;
            }
            
            DebugOut("OverlayHook: Basic Windows DLLs found");
            
            // Set DLL search path to include our DLL directory
            std::wstring dllDir = GetDllDirectory();
            if (!dllDir.empty()) {
                if (SetDllDirectoryW(dllDir.c_str())) {
                    DebugOut("OverlayHook: DLL search directory set successfully");
                } else {
                    DebugOut("OverlayHook: Failed to set DLL search directory");
                }
            }
            
            // Preload dependencies
            if (PreloadDependencies()) {
                DebugOut("OverlayHook: Dependencies preloaded successfully");
            } else {
                DebugOut("OverlayHook: Failed to preload dependencies - continuing anyway");
            }
            
            // Initialize hook manager with better error handling
            try {
                g_hookManager = new HookManager();
                DebugOut("OverlayHook: HookManager created successfully");
            } catch (const std::exception& e) {
                DebugOut("OverlayHook: EXCEPTION creating HookManager");
                g_initializationFailed = true;
                return FALSE;
            } catch (...) {
                DebugOut("OverlayHook: UNKNOWN EXCEPTION creating HookManager");
                g_initializationFailed = true;
                return FALSE;
            }
            
            // Create a thread to initialize graphics hooks after DllMain completes
            HANDLE hThread = CreateThread(NULL, 0, [](LPVOID) -> DWORD {
                DebugOut("OverlayHook: Initialization thread started");
                
                // Add delay to ensure DllMain has completed
                Sleep(500);
                
                if (g_initializationFailed) {
                    DebugOut("OverlayHook: Initialization was marked as failed, exiting thread");
                    return 0;
                }
                
                if (g_hookManager) {
                    try {
                        if (!g_hookManager->initialize()) {
                            DebugOut("OverlayHook: HookManager initialization failed");
                        } else {
                            DebugOut("OverlayHook: HookManager initialized successfully");
                        }
                    } catch (const std::exception& e) {
                        DebugOut("OverlayHook: EXCEPTION during HookManager initialization");
                    } catch (...) {
                        DebugOut("OverlayHook: UNKNOWN EXCEPTION during HookManager initialization");
                    }
                } else {
                    DebugOut("OverlayHook: ERROR - HookManager is null in initialization thread");
                }
                
                DebugOut("OverlayHook: Initialization thread completed");
                return 0;
            }, nullptr, 0, nullptr);
            
            if (!hThread) {
                DebugOut("OverlayHook: ERROR - Failed to create initialization thread");
                return FALSE;
            }
            
            // Don't wait for the thread, just close the handle
            CloseHandle(hThread);
            
            DebugOut("OverlayHook: DLL_PROCESS_ATTACH completed successfully");
        }
        break;
        
    case DLL_PROCESS_DETACH:
        DebugOut("OverlayHook: DLL_PROCESS_DETACH called - Cleaning up");
        
        if (g_hookManager) {
            try {
                g_hookManager->uninitialize();
                delete g_hookManager;
                g_hookManager = nullptr;
                DebugOut("OverlayHook: HookManager cleaned up successfully");
            } catch (...) {
                DebugOut("OverlayHook: EXCEPTION during cleanup");
            }
        }
        
        DebugOut("OverlayHook: DLL_PROCESS_DETACH completed");
        break;
    }
    return TRUE;
} 