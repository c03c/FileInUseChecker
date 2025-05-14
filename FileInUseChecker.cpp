#define UNICODE
#define _UNICODE
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <psapi.h>
#include <restartmanager.h>
#include <tchar.h>
#pragma comment(lib, "Rstrtmgr.lib")

struct ProcessInfo {
    DWORD processId;
    std::wstring processName;
    std::wstring processPath;
};

std::wstring GetProcessNameFromPath(const std::wstring& path) {
    size_t pos = path.find_last_of(L'\\');
    return (pos != std::wstring::npos) ? path.substr(pos + 1) : path;
}

// 将设备路径转换为DOS路径
std::wstring GetDosPathFromNtPath(const std::wstring& ntPath) {
    WCHAR drives[MAX_PATH] = { 0 };
    if (GetLogicalDriveStringsW(MAX_PATH, drives) == 0) {
        return ntPath;
    }

    for (WCHAR* drive = drives; *drive; drive += 4) {
        WCHAR devicePath[MAX_PATH] = { 0 };
        drive[2] = L'\\'; // 将 "C:" 改为 "C:\"
        if (QueryDosDeviceW(std::wstring(drive, 2).c_str(), devicePath, MAX_PATH) == 0) {
            continue;
        }
        drive[2] = L'\0'; // 恢复为 "C:"

        size_t devicePathLen = wcslen(devicePath);
        if (_wcsnicmp(ntPath.c_str(), devicePath, devicePathLen) == 0) {
            return drive + std::wstring(ntPath.c_str() + devicePathLen);
        }
    }
    return ntPath;
}

std::wstring GetFullPathAndRemoveQuotes(const std::wstring& path) {
    std::wstring result = path;
    if (!result.empty() && result.front() == L'"' && result.back() == L'"') {
        result = result.substr(1, result.length() - 2);
    }
    
    WCHAR fullPath[MAX_PATH];
    if (GetFullPathNameW(result.c_str(), MAX_PATH, fullPath, NULL) != 0) {
        return fullPath;
    }
    return result;
}

std::wstring FormatProcessInfo(const ProcessInfo& process) {
    return L"进程名称: " + process.processName + L"\n"
           L"进程ID: " + std::to_wstring(process.processId) + L"\n"
           L"完整路径: " + process.processPath + L"\n\n";
}

class ScopedHandle {
public:
    ScopedHandle(HANDLE h = NULL) : handle(h) {}
    ~ScopedHandle() { if (handle) CloseHandle(handle); }
    operator HANDLE() const { return handle; }
    HANDLE* operator&() { return &handle; }
    bool isValid() const { return handle != NULL && handle != INVALID_HANDLE_VALUE; }
private:
    HANDLE handle;
    // 禁止复制
    ScopedHandle(const ScopedHandle&);
    ScopedHandle& operator=(const ScopedHandle&);
};

class RestartManagerSession {
public:
    RestartManagerSession() : sessionHandle(0) {
        RmStartSession(&sessionHandle, 0, sessionKey);
    }
    ~RestartManagerSession() {
        if (sessionHandle) RmEndSession(sessionHandle);
    }
    operator DWORD() const { return sessionHandle; }
    bool isValid() const { return sessionHandle != 0; }
private:
    DWORD sessionHandle;
    WCHAR sessionKey[CCH_RM_SESSION_KEY + 1];
};

std::vector<ProcessInfo> GetProcessesUsingFile(const std::wstring& filePath) {
    std::vector<ProcessInfo> processes;
    std::wstring fullPath = GetFullPathAndRemoveQuotes(filePath);
    
    // 检查路径是否存在
    DWORD attrs = GetFileAttributesW(fullPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return processes;
    }

    // 提升权限
    HANDLE hToken;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        TOKEN_PRIVILEGES tkp = {};
        if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tkp.Privileges[0].Luid)) {
            tkp.PrivilegeCount = 1;
            tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0);
        }
        CloseHandle(hToken);
    }

    // 尝试获取更多权限
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        LUID luid;
        if (LookupPrivilegeValue(NULL, SE_BACKUP_NAME, &luid)) {
            TOKEN_PRIVILEGES tp;
            tp.PrivilegeCount = 1;
            tp.Privileges[0].Luid = luid;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
        }
        CloseHandle(hToken);
    }

    DWORD sessionHandle;
    WCHAR sessionKey[CCH_RM_SESSION_KEY + 1] = { 0 };
    DWORD error = RmStartSession(&sessionHandle, 0, sessionKey);
    if (error != ERROR_SUCCESS) {
        return processes;
    }

    // 如果是目录，扫描其中的所有文件
    if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
        WIN32_FIND_DATAW findData;
        std::wstring searchPath = fullPath + L"\\*";
        HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (wcscmp(findData.cFileName, L".") != 0 && wcscmp(findData.cFileName, L"..") != 0) {
                    std::wstring subFilePath = fullPath + L"\\" + findData.cFileName;
                    auto fileProcesses = GetProcessesUsingFile(subFilePath);
                    for (const auto& proc : fileProcesses) {
                        if (std::find_if(processes.begin(), processes.end(),
                            [&](const ProcessInfo& p) { return p.processId == proc.processId; }) == processes.end()) {
                            processes.push_back(proc);
                        }
                    }
                }
            } while (FindNextFileW(hFind, &findData));
            FindClose(hFind);
        }
        RmEndSession(sessionHandle);
        return processes;
    }

    // 尝试打开文件以检查是否被锁定
    HANDLE hFile = CreateFileW(fullPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                             NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
    }
    
    PCWSTR files[] = { fullPath.c_str() };
    error = RmRegisterResources(sessionHandle, 1, files, 0, NULL, 0, NULL);
    
    if (error == ERROR_SUCCESS) {
        UINT processInfoNeeded = 0;
        UINT processInfoCount = 16;
        std::vector<RM_PROCESS_INFO> processInfo(processInfoCount);
        DWORD reason;
        
        error = RmGetList(sessionHandle, &processInfoNeeded, &processInfoCount, processInfo.data(), &reason);
        if (error == ERROR_MORE_DATA) {
            processInfo.resize(processInfoNeeded);
            processInfoCount = processInfoNeeded;
            error = RmGetList(sessionHandle, &processInfoNeeded, &processInfoCount, processInfo.data(), &reason);
        }
        
        if (error == ERROR_SUCCESS) {
            for (UINT i = 0; i < processInfoCount; i++) {
                HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 
                                           FALSE, processInfo[i].Process.dwProcessId);
                if (hProcess) {
                    WCHAR processPath[MAX_PATH];
                    if (GetProcessImageFileNameW(hProcess, processPath, MAX_PATH) > 0) {
                        ProcessInfo pi;
                        pi.processId = processInfo[i].Process.dwProcessId;
                        pi.processPath = GetDosPathFromNtPath(processPath);
                        pi.processName = GetProcessNameFromPath(pi.processPath);
                        processes.push_back(pi);
                    }
                    CloseHandle(hProcess);
                }
            }
        }
    }
    
    RmEndSession(sessionHandle);

    // 如果没有找到进程，尝试显示错误信息以帮助诊断
    if (processes.empty()) {
        DWORD lastError = GetLastError();
        WCHAR errorMsg[256];
        swprintf_s(errorMsg, L"路径: %s\n错误代码: %d", fullPath.c_str(), lastError);
        OutputDebugStringW(errorMsg);
    }

    return processes;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == NULL || argc <= 1) {
        MessageBox(NULL, L"请通过右键菜单选择文件来使用此程序", L"提示", MB_OK | MB_ICONINFORMATION);
        LocalFree(argv);
        return 1;
    }

    std::wstring filePath = argv[1];
    LocalFree(argv);

    auto processes = GetProcessesUsingFile(filePath);
    
    if (processes.empty()) {
        MessageBox(NULL, L"没有进程正在使用此文件", L"文件使用情况", MB_OK | MB_ICONINFORMATION);
    } else {
        std::wstring message = L"以下进程正在使用此文件：\n\n";
        for (const auto& process : processes) {
            message += FormatProcessInfo(process);
        }
        MessageBox(NULL, message.c_str(), L"文件使用情况", MB_OK | MB_ICONINFORMATION);
    }

    return 0;
} 