#pragma once
// [private build] window mode: fullscreen / windowed / borderless.
//
// The game always asks Direct3D for an exclusive fullscreen device. We hook the device's
// Reset() and flip the presentation parameters to windowed, then shape the game window
// ourselves. The resolution chosen in Options -> Display is used as the window size.
// In the two windowed modes the game keeps running while it is in the background.
//
// Setting: CoopAndreas\settings.ini  [window] mode = 0 (fullscreen) | 1 (windowed) | 2 (borderless)
// F11 cycles through the modes in game.
class CWindowMode
{
public:
    enum eMode
    {
        MODE_FULLSCREEN = 0,
        MODE_WINDOWED = 1,
        MODE_BORDERLESS = 2,
    };

    static int ms_nMode;

    static void Init();              // call once at startup (reads the setting, registers events)
    static void Apply(int mode);     // switch mode now (also saves it)
    static void Process();           // per frame: F11, deferred window fix
    static bool IsWindowed() { return ms_nMode != MODE_FULLSCREEN; }

private:
    static void OnRwInitialized();
    static void HookDevice();
    static void FixWindow();
    static void ForceVideoModeReset();
    static void SetRunInBackground(bool enable);
    static std::string SettingsPath();
    static void LoadSetting();
    static void SaveSetting();
};
