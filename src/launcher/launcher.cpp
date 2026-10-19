#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>

// launcher.exe
//  1. Starts the Minecraft Bedrock UWP app (shell:AppsFolder AUMID)
//  2. Waits for the client process
//  3. Injects hook.dll into it

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")

static void EnableDebugPrivilege() {
    HANDLE tok;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok)) return;
    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    if (LookupPrivilegeValueW(nullptr, SE_DEBUG_NAME, &tp.Privileges[0].Luid)) {
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        AdjustTokenPrivileges(tok, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    }
    CloseHandle(tok);
}

static DWORD FindMinecraftPid() {
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    DWORD pid = 0;
    if (snap != INVALID_HANDLE_VALUE && Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"Minecraft.Windows.exe") == 0) { pid = pe.th32ProcessID; break; }
        } while (Process32NextW(snap, &pe));
    }
    if (snap != INVALID_HANDLE_VALUE) CloseHandle(snap);
    return pid;
}

static BOOL Inject(DWORD pid, const wchar_t* dllPath) {
    HANDLE h = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!h) return FALSE;
    size_t len = (wcslen(dllPath) + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(h, nullptr, len, MEM_COMMIT, PAGE_READWRITE);
    if (!remote) { CloseHandle(h); return FALSE; }
    if (!WriteProcessMemory(h, remote, dllPath, len, nullptr)) { CloseHandle(h); return FALSE; }
    FARPROC loadLib = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    HANDLE t = CreateRemoteThread(h, nullptr, 0, (LPTHREAD_START_ROUTINE)loadLib, remote, 0, nullptr);
    BOOL ok = t != nullptr;
    if (t) {
        WaitForSingleObject(t, 10000);
        CloseHandle(t);
    }
    CloseHandle(h);
    return ok;
}

int wmain(int argc, wchar_t** argv) {
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    // expect hook.dll next to launcher.exe, or pass its path as argv[1]
    wchar_t dllPath[MAX_PATH];
    if (argc > 1) {
        wcscpy_s(dllPath, argv[1]);
    } else {
        wcscpy_s(dllPath, exe);
        wcscat_s(dllPath, L".dll");
    }

    EnableDebugPrivilege();

    // Launch the UWP app
    wchar_t cmd[512] = L"explorer.exe shell:AppsFolder\\MICROSOFT.MINECRAFTUWP_8wekyb3d8bbwe!App";
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    if (!CreateProcessW(nullptr, cmd, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        wprintf(L"failed to launch minecraft (%lu)\n", GetLastError());
        return 1;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // Wait for the client process to appear
    DWORD pid = 0;
    for (int i = 0; i < 60; ++i) {
        Sleep(1000);
        pid = FindMinecraftPid();
        if (pid) break;
    }
    if (!pid) { wprintf(L"Minecraft.Windows.exe not found\n"); return 1; }
    wprintf(L"pid=%lu injecting %s\n", pid, dllPath);
    if (!Inject(pid, dllPath)) {
        wprintf(L"inject failed (%lu). Try running launcher.exe as Administrator.\n", GetLastError());
        return 1;
    }
    wprintf(L"injected - hook will click Play in ~12s\n");
    return 0;
}
