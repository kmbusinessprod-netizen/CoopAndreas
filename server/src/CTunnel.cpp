#include "stdafx.h"
#include "CTunnel.h"

#if defined(_WIN32)
#include <windows.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <string>
#include <vector>

#pragma comment(lib, "shell32.lib")

static bool IsProcessRunning(const wchar_t* exeName)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return false;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool found = false;
    if (Process32FirstW(snap, &entry))
    {
        do
        {
            if (_wcsicmp(entry.szExeFile, exeName) == 0)
            {
                found = true;
                break;
            }
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return found;
}

static std::wstring Widen(const std::string& s)
{
    if (s.empty())
        return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(len > 0 ? len - 1 : 0, L'\0');
    if (len > 1)
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], len);
    return w;
}

static std::wstring ExpandEnv(const wchar_t* path)
{
    wchar_t buf[MAX_PATH * 2];
    DWORD n = ExpandEnvironmentStringsW(path, buf, ARRAYSIZE(buf));
    return (n > 0 && n <= ARRAYSIZE(buf)) ? std::wstring(buf) : std::wstring(path);
}

static bool FileExists(const std::wstring& path)
{
    DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

static void EnsurePlayitRunning()
{
    if (IsProcessRunning(L"playit.exe"))
    {
        printf("[Tunel] : playit ya esta funcionando.\n");
        return;
    }

    std::vector<std::wstring> candidates;
    std::string configured = CConfigManager::GetConfigString("playit_path");
    if (!configured.empty())
        candidates.push_back(Widen(configured));
    candidates.push_back(ExpandEnv(L"%ProgramFiles%\\playit_gg\\bin\\playit.exe"));
    candidates.push_back(ExpandEnv(L"%ProgramFiles(x86)%\\playit_gg\\bin\\playit.exe"));
    candidates.push_back(ExpandEnv(L"%LOCALAPPDATA%\\Programs\\playit_gg\\bin\\playit.exe"));
    candidates.push_back(ExpandEnv(L"%LOCALAPPDATA%\\playit_gg\\bin\\playit.exe"));

    for (const auto& path : candidates)
    {
        if (!FileExists(path))
            continue;

        HINSTANCE res = ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWMINNOACTIVE);
        if ((INT_PTR)res > 32)
        {
            printf("[Tunel] : Abri playit automaticamente.\n");
            return;
        }
    }

    printf("[Tunel] : No encontre playit abierto. Abrelo a mano (o agrega 'playit_path = ruta\\playit.exe'\n");
    printf("[Tunel] : en server-config.ini para que se abra solo).\n");
}

bool CTunnel::Start()
{
    std::string tunnel = CConfigManager::GetConfigString("tunnel");
    if (tunnel.empty())
        return false;

    EnsurePlayitRunning();

    printf("\n");
    printf("  ==========================================================\n");
    printf("   Pasale a tus amigos:   %s\n", tunnel.c_str());
    printf("   (pueden pegarlo entero en el campo IP con Ctrl+V)\n");
    printf("   Tu, en esta PC, conectate a 127.0.0.1 puerto %u\n", CConfigManager::GetConfigPort());
    printf("  ==========================================================\n\n");
    return true;
}

#else
bool CTunnel::Start()
{
    return false;
}
#endif
