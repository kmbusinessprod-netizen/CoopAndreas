#pragma once
#include <string>
#include <vector>

// [private build] imported skins: every pair NAME.dff + NAME.txd in CoopAndreas\skins\ becomes a skin.
// At startup they are packed into CoopAndreas\coop_skins.img (renamed csk00, csk01... so they never
// replace a ped of the game) and that IMG is registered with the game's streaming, exactly like the
// game's own IMG files. They are then loaded like story characters (special slots), so any ped skin made
// for the game works the same way. Skin id = 2000 + hash of the file name: the same on every PC that
// has the same file.
class CSkinImport
{
public:
    struct Entry
    {
        std::string display;   // file name without extension, as the user named it
        std::string internal;  // name inside coop_skins.img
        uint16_t id;
    };

    static void Install();  // hooks the streaming init
    static const std::vector<Entry>& GetAll() { return ms_entries; }
    static const Entry* Find(uint16_t id);
    static bool IsImportedId(int id) { return id >= 2000; }
    static bool BuildImageFor(const std::string& gameDir) { return BuildImage(gameDir); }

private:
    static std::vector<Entry> ms_entries;
    static bool BuildImage(const std::string& gameDir);
};
