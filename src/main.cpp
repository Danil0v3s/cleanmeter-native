#include <Windows.h>
#include <TlHelp32.h>
#include <iostream>
#include <string>
#include <conio.h>

// Function to find a process by name
DWORD findProcessByName(const std::wstring& processName) {
    DWORD processId = 0;
    
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        std::wcout << L"Failed to create process snapshot" << std::endl;
        return 0;
    }
    
    PROCESSENTRY32W processEntry;
    processEntry.dwSize = sizeof(PROCESSENTRY32W);
    
    if (Process32FirstW(hSnapshot, &processEntry)) {
        do {
            if (_wcsicmp(processEntry.szExeFile, processName.c_str()) == 0) {
                processId = processEntry.th32ProcessID;
                std::wcout << L"Found " << processName << L" with PID: " << processId << std::endl;
                break;
            }
        } while (Process32NextW(hSnapshot, &processEntry));
    }
    
    CloseHandle(hSnapshot);
    return processId;
}

// Function to wait for a process to appear
DWORD waitForProcess(const std::wstring& processName, int timeoutSeconds = 30) {
    std::wcout << L"Waiting for " << processName << L" to start..." << std::endl;
    
    int attempts = 0;
    int maxAttempts = timeoutSeconds;
    
    while (attempts < maxAttempts) {
        DWORD pid = findProcessByName(processName);
        if (pid != 0) {
            return pid;
        }
        
        std::wcout << L"Process not found, waiting... (" << (attempts + 1) << L"/" << maxAttempts << L")" << std::endl;
        Sleep(1000);  // Wait 1 second
        attempts++;
    }
    
    std::wcout << L"Timeout waiting for " << processName << std::endl;
    return 0;
}

class ProcessInjector {
private:
    DWORD pid_;
    std::wstring dllPath_;

public:
    ProcessInjector(DWORD pid) : pid_(pid) {
        // Get the current executable directory
        wchar_t modulePath[MAX_PATH];
        GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
        
        // Remove filename to get directory
        std::wstring exeDir(modulePath);
        size_t lastSlash = exeDir.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            exeDir = exeDir.substr(0, lastSlash);
        }
        
        dllPath_ = exeDir + L"\\OverlayHook.dll";
        
        // Check if DLL exists and get file info
        DWORD fileAttr = GetFileAttributesW(dllPath_.c_str());
        if (fileAttr == INVALID_FILE_ATTRIBUTES) {
            std::wcout << L"ERROR: DLL not found at " << dllPath_ << std::endl;
            std::wcout << L"GetFileAttributes error: " << GetLastError() << std::endl;
        } else {
            std::wcout << L"DLL found at: " << dllPath_ << std::endl;
            
            // Get file size for additional verification
            WIN32_FIND_DATAW findData;
            HANDLE hFind = FindFirstFileW(dllPath_.c_str(), &findData);
            if (hFind != INVALID_HANDLE_VALUE) {
                LARGE_INTEGER fileSize;
                fileSize.LowPart = findData.nFileSizeLow;
                fileSize.HighPart = findData.nFileSizeHigh;
                std::wcout << L"DLL size: " << fileSize.QuadPart << L" bytes" << std::endl;
                FindClose(hFind);
            }
        }
    }

    bool injectDll() {
        HANDLE hProcess = nullptr;
        HANDLE hThread = nullptr;
        LPVOID pRemoteString = nullptr;
        bool success = false;

        std::wcout << L"Starting DLL injection process..." << std::endl;
        std::wcout << L"Target PID: " << pid_ << std::endl;
        std::wcout << L"DLL Path: " << dllPath_ << std::endl;

        try {
            // Open target process
            std::wcout << L"Opening target process..." << std::endl;
            hProcess = OpenProcess(
                PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                FALSE, pid_);
            
            if (!hProcess) {
                DWORD openError = GetLastError();
                std::wcout << L"Failed to open process " << pid_ << L". Error: " << openError << std::endl;
                
                switch (openError) {
                case ERROR_ACCESS_DENIED:
                    std::wcout << L"  ERROR_ACCESS_DENIED: Insufficient privileges to access the target process." << std::endl;
                    std::wcout << L"  Solutions:" << std::endl;
                    std::wcout << L"    - Run the injector as Administrator" << std::endl;
                    std::wcout << L"    - Target process may be running with higher privileges" << std::endl;
                    std::wcout << L"    - Some protected processes cannot be accessed" << std::endl;
                    break;
                case ERROR_INVALID_PARAMETER:
                    std::wcout << L"  ERROR_INVALID_PARAMETER: Invalid process ID or process may have terminated." << std::endl;
                    break;
                default:
                    std::wcout << L"  Unknown error code: " << openError << std::endl;
                    break;
                }
                return false;
            }
            std::wcout << L"Successfully opened target process." << std::endl;

            // Calculate string length
            size_t dllPathSize = (dllPath_.length() + 1) * sizeof(wchar_t);

            // Allocate memory in target process
            pRemoteString = VirtualAllocEx(hProcess, nullptr, dllPathSize, MEM_COMMIT, PAGE_READWRITE);
            if (!pRemoteString) {
                std::wcout << L"Failed to allocate memory in target process. Error: " << GetLastError() << std::endl;
                throw std::runtime_error("VirtualAllocEx failed");
            }

            // Write DLL path to target process
            if (!WriteProcessMemory(hProcess, pRemoteString, dllPath_.c_str(), dllPathSize, nullptr)) {
                std::wcout << L"Failed to write DLL path to target process. Error: " << GetLastError() << std::endl;
                throw std::runtime_error("WriteProcessMemory failed");
            }

            // Get LoadLibraryW address
            HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
            if (!hKernel32) {
                std::wcout << L"Failed to get kernel32.dll handle" << std::endl;
                throw std::runtime_error("GetModuleHandle failed");
            }

            LPTHREAD_START_ROUTINE pLoadLibrary = (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel32, "LoadLibraryW");
            if (!pLoadLibrary) {
                std::wcout << L"Failed to get LoadLibraryW address" << std::endl;
                throw std::runtime_error("GetProcAddress failed");
            }

            // Create remote thread
            hThread = CreateRemoteThread(hProcess, nullptr, 0, pLoadLibrary, pRemoteString, 0, nullptr);
            if (!hThread) {
                std::wcout << L"Failed to create remote thread. Error: " << GetLastError() << std::endl;
                throw std::runtime_error("CreateRemoteThread failed");
            }

            // Wait for thread to complete
            std::wcout << L"Waiting for remote thread to complete..." << std::endl;
            DWORD waitResult = WaitForSingleObject(hThread, 5000); // 5 second timeout
            
            if (waitResult == WAIT_TIMEOUT) {
                std::wcout << L"Remote thread timed out!" << std::endl;
                throw std::runtime_error("Remote thread timeout");
            } else if (waitResult == WAIT_FAILED) {
                std::wcout << L"Failed to wait for remote thread. Error: " << GetLastError() << std::endl;
                throw std::runtime_error("WaitForSingleObject failed");
            }

            // Get thread exit code
            DWORD exitCode;
            if (!GetExitCodeThread(hThread, &exitCode)) {
                std::wcout << L"Failed to get thread exit code. Error: " << GetLastError() << std::endl;
                throw std::runtime_error("GetExitCodeThread failed");
            }
            
            std::wcout << L"Remote thread completed with exit code: 0x" << std::hex << exitCode << std::dec << std::endl;
            
            if (exitCode == 0) {
                std::wcout << L"DLL injection failed - LoadLibrary returned NULL" << std::endl;
                
                // Get the last error from the target process
                // This is tricky since we can't get the target's GetLastError directly
                // But we can check common issues
                // std::wcout << L"Common causes:" << std::endl;
                // std::wcout << L"1. DLL or its dependencies not found" << std::endl;
                // std::wcout << L"2. Architecture mismatch (x64/x86)" << std::endl;
                // std::wcout << L"3. DLL's DllMain returned FALSE" << std::endl;
                // std::wcout << L"4. Missing runtime libraries (Visual C++ Redistributable)" << std::endl;
                
                // Try to check if the DLL can be loaded in our own process
                std::wcout << L"\nTesting DLL loading in current process..." << std::endl;
                HMODULE testModule = LoadLibraryW(dllPath_.c_str());
                if (testModule) {
                    std::wcout << L"DLL loads successfully in current process" << std::endl;
                    FreeLibrary(testModule);
                } else {
                    DWORD loadError = GetLastError();
                    std::wcout << L"DLL fails to load even in current process. Error: " << loadError << std::endl;
                    
                    // Provide more specific error information
                    switch (loadError) {
                    case ERROR_MOD_NOT_FOUND:
                        std::wcout << L"  ERROR_MOD_NOT_FOUND: The specified module or one of its dependencies could not be found." << std::endl;
                        std::wcout << L"  This means OverlayHook.dll exists but depends on missing libraries." << std::endl;
                        std::wcout << L"  Common missing dependencies:" << std::endl;
                        std::wcout << L"    - FW1FontWrapper.dll" << std::endl;
                        std::wcout << L"    - Visual C++ Redistributable" << std::endl;
                        std::wcout << L"    - DirectX runtime libraries" << std::endl;
                        break;
                    case ERROR_PROC_NOT_FOUND:
                        std::wcout << L"  ERROR_PROC_NOT_FOUND: The specified procedure could not be found." << std::endl;
                        break;
                    case ERROR_BAD_EXE_FORMAT:
                        std::wcout << L"  ERROR_BAD_EXE_FORMAT: The image is either not designed to run on Windows or it contains an error." << std::endl;
                        break;
                    default:
                        std::wcout << L"  Unknown error code: " << loadError << std::endl;
                        break;
                    }
                }
                
                // Check if we can even access the target process memory (diagnostic only)
                std::wcout << L"\nVerifying DLL path was written correctly (diagnostic)..." << std::endl;
                wchar_t readBuffer[MAX_PATH] = {0};
                SIZE_T bytesRead = 0;
                if (ReadProcessMemory(hProcess, pRemoteString, readBuffer, (dllPath_.length() + 1) * sizeof(wchar_t), &bytesRead)) {
                    std::wcout << L"Read from target process: " << readBuffer << std::endl;
                    if (wcscmp(readBuffer, dllPath_.c_str()) == 0) {
                        std::wcout << L"DLL path was written correctly to target process" << std::endl;
                    } else {
                        std::wcout << L"DLL path in target process doesn't match!" << std::endl;
                    }
                } else {
                    DWORD readError = GetLastError();
                    std::wcout << L"Failed to read back DLL path from target process. Error: " << readError << std::endl;
                    if (readError == ERROR_ACCESS_DENIED) {
                        std::wcout << L"  This is likely due to insufficient privileges (target process may be elevated)" << std::endl;
                        std::wcout << L"  Try running the injector as Administrator" << std::endl;
                    }
                    std::wcout << L"  Note: This is just a diagnostic check - it doesn't affect injection success" << std::endl;
                }
            } else {
                std::wcout << L"DLL injection successful! Module handle: 0x" << std::hex << exitCode << std::dec << std::endl;
                std::wcout << L"LoadLibrary returned a valid module handle, indicating successful injection." << std::endl;
                success = true;
            }

        } catch (const std::exception& e) {
            std::wcout << L"Exception during injection: " << e.what() << std::endl;
        }

        // Cleanup
        if (pRemoteString) {
            VirtualFreeEx(hProcess, pRemoteString, 0, MEM_RELEASE);
        }
        if (hThread) {
            CloseHandle(hThread);
        }
        if (hProcess) {
            CloseHandle(hProcess);
        }

        return success;
    }

    DWORD getMainThreadId() {
        DWORD mainThreadId = 0;
        ULONGLONG minCreateTime = MAXULONGLONG;

        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (hSnapshot != INVALID_HANDLE_VALUE) {
            THREADENTRY32 te32;
            te32.dwSize = sizeof(THREADENTRY32);

            if (Thread32First(hSnapshot, &te32)) {
                do {
                    if (te32.th32OwnerProcessID == pid_) {
                        HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, te32.th32ThreadID);
                        if (hThread) {
                            FILETIME createTime, exitTime, kernelTime, userTime;
                            if (GetThreadTimes(hThread, &createTime, &exitTime, &kernelTime, &userTime)) {
                                ULONGLONG threadCreateTime = ((ULONGLONG)createTime.dwHighDateTime << 32) | createTime.dwLowDateTime;
                                if (threadCreateTime < minCreateTime) {
                                    minCreateTime = threadCreateTime;
                                    mainThreadId = te32.th32ThreadID;
                                }
                            }
                            CloseHandle(hThread);
                        }
                    }
                } while (Thread32Next(hSnapshot, &te32));
            }
            CloseHandle(hSnapshot);
        }

        return mainThreadId;
    }
};

bool processExists(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (hProcess) {
        CloseHandle(hProcess);
        return true;
    }
    return false;
}

bool isProcess64Bit(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hProcess) {
        return false;
    }

    BOOL isWow64 = FALSE;
    if (IsWow64Process(hProcess, &isWow64)) {
        CloseHandle(hProcess);
        return !isWow64;  // If not WoW64, then it's native 64-bit
    }
    
    CloseHandle(hProcess);
    return false;
}

int main(int argc, char* argv[]) {
    std::wcout << L"OverlayInjector - Waiting for Hollow Knight..." << std::endl;
    std::wcout << L"=========================================" << std::endl;

    // Target process name
    std::wstring targetProcess = L"hollow_knight.exe";
    
    // Find the target process
    DWORD pid = findProcessByName(targetProcess);
    if (pid == 0) {
        // Process not found, wait for it to start
        pid = waitForProcess(targetProcess, 60); // Wait up to 60 seconds
        if (pid == 0) {
            std::wcout << L"Failed to find " << targetProcess << L" process." << std::endl;
            std::wcout << L"Make sure Hollow Knight is running and try again." << std::endl;
            std::wcout << L"Press any key to exit..." << std::endl;
            _getch();
            return 1;
        }
    }
    
    // Check if process exists and is accessible
    if (!processExists(pid)) {
        std::wcout << L"Process " << pid << L" does not exist or cannot be accessed." << std::endl;
        return 1;
    }
    
    // Check process architecture
    bool targetIs64Bit = isProcess64Bit(pid);
    bool injectorIs64Bit = sizeof(void*) == 8;
    
    std::wcout << L"Target process " << pid << L" architecture: " << (targetIs64Bit ? L"x64" : L"x86") << std::endl;
    std::wcout << L"Injector architecture: " << (injectorIs64Bit ? L"x64" : L"x86") << std::endl;
    
    if (targetIs64Bit != injectorIs64Bit) {
        std::wcout << L"ARCHITECTURE MISMATCH!" << std::endl;
        std::wcout << L"Cannot inject " << (injectorIs64Bit ? L"x64" : L"x86") << L" DLL into " 
                  << (targetIs64Bit ? L"x64" : L"x86") << L" process." << std::endl;
        std::wcout << L"Press any key to exit..." << std::endl;
        _getch();
        return 1;
    }
    
    std::wcout << L"Architecture match confirmed. Proceeding with injection..." << std::endl;
    std::wcout << L"Injecting overlay into " << targetProcess << L" (PID: " << pid << L")" << std::endl;

    ProcessInjector injector(pid);
    
    if (injector.injectDll()) {
        std::wcout << L"Injection completed successfully!" << std::endl;
        std::wcout << L"The overlay should now be active in Hollow Knight." << std::endl;
        std::wcout << L"Press any key to exit..." << std::endl;
        _getch();
        return 0;
    } else {
        std::wcout << L"Injection failed!" << std::endl;
        std::wcout << L"Press any key to exit..." << std::endl;
        _getch();
        return 1;
    }
} 