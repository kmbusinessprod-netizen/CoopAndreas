#pragma once
// [private build] plays .wav/.mp3 files from CoopAndreas\sounds through bass.dll (the copy CLEO already
// ships next to gta_sa.exe). Volume follows the game's SFX volume and the distance to the camera.
class CSoundFile
{
public:
    static unsigned int Play(const char* file, bool loop = false);  // returns a handle, 0 if it failed
    static void Stop(unsigned int handle);
    static bool IsPlaying(unsigned int handle);
    // 3D-ish: volume by distance (silent beyond maxDistance) and left/right pan from the camera
    static void Place(unsigned int handle, const CVector& pos, float maxDistance = 90.0f);
    static void SetVolume(unsigned int handle, float volume);
};
