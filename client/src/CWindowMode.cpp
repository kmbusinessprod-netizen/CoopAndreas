#include "stdafx.h"
#include "CWindowMode.h"
#include <d3d9.h>

int CWindowMode::ms_nMode = CWindowMode::MODE_FULLSCREEN;

typedef HRESULT(__stdcall* Reset_t)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
static Reset_t s_pOriginalReset = nullptr;
static bool s_bDeviceHooked = false;
static bool s_bFixWindowNextFrame = false;
static int s_nFixWindowFrames = 0;
static bool s_bRunInBackground = false;
static int s_nBackBufferWidth = 0;
static int s_nBackBufferHeight = 0;

static HWND GameWindow()
{
    return RsGlobal.ps ? RsGlobal.ps->window : nullptr;
}

std::string CWindowMode::SettingsPath()
{
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash)
        *slash = '\0';
    return std::string(path) + "\\CoopAndreas\\settings.ini";
}

void CWindowMode::LoadSetting()
{
    ms_nMode = GetPrivateProfileIntA("window", "mode", MODE_FULLSCREEN, SettingsPath().c_str());
    if (ms_nMode < MODE_FULLSCREEN || ms_nMode > MODE_BORDERLESS)
        ms_nMode = MODE_FULLSCREEN;
}

void CWindowMode::SaveSetting()
{
    std::string path = SettingsPath();
    WritePrivateProfileStringA("window", "mode", std::to_string(ms_nMode).c_str(), path.c_str());
    WritePrivateProfileStringA("window", "; 0 = pantalla completa, 1 = ventana, 2 = ventana sin bordes (F11 en el juego)", "", path.c_str());
}

// ------------------------------------------------------------------------------------------
// window shaping

void CWindowMode::FixWindow()
{
    HWND hwnd = GameWindow();
    if (hwnd == nullptr || !IsWindowed())
        return;

    int width = s_nBackBufferWidth > 0 ? s_nBackBufferWidth : RsGlobal.maximumWidth;
    int height = s_nBackBufferHeight > 0 ? s_nBackBufferHeight : RsGlobal.maximumHeight;

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoA(monitor, &mi);
    RECT& area = mi.rcMonitor;

    if (ms_nMode == MODE_BORDERLESS)
    {
        SetWindowLongA(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowLongA(hwnd, GWL_EXSTYLE, 0);

        // a window as big as the screen covers it like fullscreen; a smaller one is centred
        int x = area.left + ((area.right - area.left) - width) / 2;
        int y = area.top + ((area.bottom - area.top) - height) / 2;
        SetWindowPos(hwnd, HWND_TOP, x, y, width, height, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
    else
    {
        DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE;
        SetWindowLongA(hwnd, GWL_STYLE, style);
        SetWindowLongA(hwnd, GWL_EXSTYLE, 0);

        RECT rc{0, 0, width, height};
        AdjustWindowRect(&rc, style, FALSE);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        RECT& work = mi.rcWork;
        int x = work.left + ((work.right - work.left) - w) / 2;
        int y = work.top + ((work.bottom - work.top) - h) / 2;
        if (y < work.top)
            y = work.top;
        SetWindowPos(hwnd, HWND_NOTOPMOST, x, y, w, h, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
}

// ------------------------------------------------------------------------------------------
// device hook

static HRESULT __stdcall Reset_Hook(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* pp)
{
    if (pp && CWindowMode::IsWindowed())
    {
        pp->Windowed = TRUE;
        pp->FullScreen_RefreshRateInHz = 0;
        if (pp->hDeviceWindow == nullptr)
            pp->hDeviceWindow = GameWindow();
        // only these intervals are legal in windowed mode
        if (pp->PresentationInterval != D3DPRESENT_INTERVAL_IMMEDIATE && pp->PresentationInterval != D3DPRESENT_INTERVAL_ONE)
            pp->PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
        s_nBackBufferWidth = pp->BackBufferWidth;
        s_nBackBufferHeight = pp->BackBufferHeight;
    }
    else if (pp)
    {
        s_nBackBufferWidth = pp->BackBufferWidth;
        s_nBackBufferHeight = pp->BackBufferHeight;
    }

    HRESULT hr = s_pOriginalReset(device, pp);

    if (SUCCEEDED(hr) && CWindowMode::IsWindowed())
    {
        if (RsGlobal.ps)
            RsGlobal.ps->fullScreen = FALSE;
        s_bFixWindowNextFrame = true;
        s_nFixWindowFrames = 3;  // RenderWare touches the window after the reset too; insist a few frames
    }
    else if (SUCCEEDED(hr) && RsGlobal.ps)
    {
        RsGlobal.ps->fullScreen = TRUE;
    }

    return hr;
}

void CWindowMode::HookDevice()
{
    if (s_bDeviceHooked)
        return;

    IDirect3DDevice9* device = static_cast<IDirect3DDevice9*>(RwD3D9GetCurrentD3DDevice());
    if (device == nullptr)
        return;

    void** vtable = *reinterpret_cast<void***>(device);
    DWORD old;
    VirtualProtect(&vtable[16], sizeof(void*), PAGE_EXECUTE_READWRITE, &old);
    s_pOriginalReset = reinterpret_cast<Reset_t>(vtable[16]);
    vtable[16] = reinterpret_cast<void*>(Reset_Hook);
    VirtualProtect(&vtable[16], sizeof(void*), old, &old);

    s_bDeviceHooked = true;
}

// RwD3D9ChangeVideoMode() returns early when the mode is the current one; clear the
// "current mode" global so it goes through the full release / Reset / restore path
void CWindowMode::ForceVideoModeReset()
{
    int current = RwEngineGetCurrentVideoMode();
    int& rwCurrentMode = *reinterpret_cast<int*>(0xC97C18);
    rwCurrentMode = -1;
    if (!RwD3D9ChangeVideoMode(current))
    {
        rwCurrentMode = current;
        printf("[window] RwD3D9ChangeVideoMode(%d) failed\n", current);
    }
}

// main loop: `je skipFrame` when ForegroundApp == 0 (0x748A8D). NOP it and the game keeps
// simulating and rendering while another window has the focus.
void CWindowMode::SetRunInBackground(bool enable)
{
    if (s_bRunInBackground == enable)
        return;
    s_bRunInBackground = enable;

    if (enable)
        patch::Nop(0x748A8D, 6);
    else
    {
        static const unsigned char original[6] = {0x0F, 0x84, 0x20, 0x03, 0x00, 0x00};
        patch::SetRaw(0x748A8D, (void*)original, 6);
    }
}

// ------------------------------------------------------------------------------------------

void CWindowMode::OnRwInitialized()
{
    HookDevice();
    if (IsWindowed())
    {
        ForceVideoModeReset();
        SetRunInBackground(true);
    }
}

void CWindowMode::Apply(int mode)
{
    if (mode < MODE_FULLSCREEN || mode > MODE_BORDERLESS)
        return;

    ms_nMode = mode;
    SaveSetting();

    HookDevice();
    ForceVideoModeReset();
    SetRunInBackground(IsWindowed());

    if (ms_nMode == MODE_FULLSCREEN)
    {
        // RenderWare restyles the window itself for exclusive modes; make sure it is on top
        if (HWND hwnd = GameWindow())
            SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW);
    }

    static const char* names[] = {"pantalla completa", "ventana", "ventana sin bordes"};
    CHud::SetHelpMessage((std::string("Modo de pantalla: ~y~") + names[ms_nMode]).c_str(), true, false, false);
}

void CWindowMode::Process()
{
    if (s_bFixWindowNextFrame)
    {
        FixWindow();
        if (--s_nFixWindowFrames <= 0)
            s_bFixWindowNextFrame = false;
    }

    // F11: next mode
    if (CPad::NewKeyState.FKeys[10] && !CPad::OldKeyState.FKeys[10] && !CChat::m_bInputActive)
        Apply((ms_nMode + 1) % 3);
}

void CWindowMode::Init()
{
    LoadSetting();
    Events::initRwEvent += OnRwInitialized;
    Events::gameProcessEvent += Process;
}
