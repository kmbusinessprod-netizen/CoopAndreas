#include "stdafx.h"
#include "CSoundFile.h"

namespace
{
using BASS_Init_t = int(WINAPI*)(int, DWORD, DWORD, HWND, const void*);
using BASS_StreamCreateFile_t = DWORD(WINAPI*)(int, const void*, unsigned long long, unsigned long long, DWORD);
using BASS_ChannelPlay_t = int(WINAPI*)(DWORD, int);
using BASS_ChannelStop_t = int(WINAPI*)(DWORD);
using BASS_StreamFree_t = int(WINAPI*)(DWORD);
using BASS_ChannelIsActive_t = DWORD(WINAPI*)(DWORD);
using BASS_ChannelSetAttribute_t = int(WINAPI*)(DWORD, DWORD, float);

struct Bass
{
    bool tried = false, ok = false;
    BASS_Init_t Init;
    BASS_StreamCreateFile_t StreamCreateFile;
    BASS_ChannelPlay_t ChannelPlay;
    BASS_ChannelStop_t ChannelStop;
    BASS_StreamFree_t StreamFree;
    BASS_ChannelIsActive_t ChannelIsActive;
    BASS_ChannelSetAttribute_t ChannelSetAttribute;
} g;

constexpr DWORD BASS_SAMPLE_LOOP = 4, BASS_STREAM_AUTOFREE = 0x40000, BASS_ATTRIB_VOL = 2, BASS_ATTRIB_PAN = 3;

bool Ready()
{
    if (g.tried)
        return g.ok;
    g.tried = true;
    HMODULE h = GetModuleHandleA("bass.dll");
    if (!h)
        h = LoadLibraryA("bass.dll");
    if (!h)
    {
        printf("[sound] bass.dll not found\n");
        return false;
    }
    g.Init = (BASS_Init_t)GetProcAddress(h, "BASS_Init");
    g.StreamCreateFile = (BASS_StreamCreateFile_t)GetProcAddress(h, "BASS_StreamCreateFile");
    g.ChannelPlay = (BASS_ChannelPlay_t)GetProcAddress(h, "BASS_ChannelPlay");
    g.ChannelStop = (BASS_ChannelStop_t)GetProcAddress(h, "BASS_ChannelStop");
    g.StreamFree = (BASS_StreamFree_t)GetProcAddress(h, "BASS_StreamFree");
    g.ChannelIsActive = (BASS_ChannelIsActive_t)GetProcAddress(h, "BASS_ChannelIsActive");
    g.ChannelSetAttribute = (BASS_ChannelSetAttribute_t)GetProcAddress(h, "BASS_ChannelSetAttribute");
    if (!g.Init || !g.StreamCreateFile || !g.ChannelPlay || !g.ChannelStop || !g.StreamFree || !g.ChannelIsActive ||
        !g.ChannelSetAttribute)
        return false;
    // CLEO may have initialised it already (then this just fails with "already", which is fine)
    g.Init(-1, 44100, 0, *(HWND*)0xC97C1C, nullptr);  // PSGLOBAL(window)
    g.ok = true;
    return true;
}

float GameVolume()
{
    return std::max(0.0f, std::min(1.0f, FrontEndMenuManager.m_nPrefsSfxVolume / 64.0f));
}
}  // namespace

unsigned int CSoundFile::Play(const char* file, bool loop)
{
    if (!Ready())
        return 0;
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash)
        sprintf_s(slash + 1, MAX_PATH - (slash + 1 - path), "CoopAndreas\\sounds\\%s", file);
    DWORD h = g.StreamCreateFile(0, path, 0, 0, (loop ? BASS_SAMPLE_LOOP : 0) | BASS_STREAM_AUTOFREE);
    if (!h)
    {
        printf("[sound] can't open %s\n", path);
        return 0;
    }
    g.ChannelSetAttribute(h, BASS_ATTRIB_VOL, GameVolume());
    g.ChannelPlay(h, 0);
    return h;
}

void CSoundFile::Stop(unsigned int handle)
{
    if (handle && Ready())
    {
        g.ChannelStop(handle);
        g.StreamFree(handle);
    }
}

bool CSoundFile::IsPlaying(unsigned int handle)
{
    return handle && Ready() && g.ChannelIsActive(handle) == 1;
}

void CSoundFile::SetVolume(unsigned int handle, float volume)
{
    if (handle && Ready())
        g.ChannelSetAttribute(handle, BASS_ATTRIB_VOL, volume * GameVolume());
}

void CSoundFile::Place(unsigned int handle, const CVector& pos, float maxDistance)
{
    if (!handle || !Ready())
        return;
    CVector cam = TheCamera.GetPosition();
    CVector d = pos - cam;
    float dist = d.Magnitude();
    float vol = std::max(0.0f, 1.0f - dist / maxDistance);
    g.ChannelSetAttribute(handle, BASS_ATTRIB_VOL, vol * vol * GameVolume());
    // pan: which side of the camera's right vector
    CMatrix& m = TheCamera.m_mCameraMatrix;
    float side = dist > 0.1f ? (d.x * m.right.x + d.y * m.right.y + d.z * m.right.z) / dist : 0.0f;
    g.ChannelSetAttribute(handle, BASS_ATTRIB_PAN, std::max(-1.0f, std::min(1.0f, side)));
}
