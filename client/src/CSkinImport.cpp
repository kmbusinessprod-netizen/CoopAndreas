#include "stdafx.h"
#include "CSkinImport.h"
#include <algorithm>
#include <fstream>
#include <map>

std::vector<CSkinImport::Entry> CSkinImport::ms_entries;

static std::string Lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)tolower(c); });
    return s;
}

static uint16_t SkinIdFor(const std::string& name)
{
    uint32_t h = 2166136261u;  // FNV-1a
    for (char c : Lower(name))
    {
        h ^= (uint8_t)c;
        h *= 16777619u;
    }
    return (uint16_t)(2000 + h % 60000);
}

const CSkinImport::Entry* CSkinImport::Find(uint16_t id)
{
    for (const Entry& e : ms_entries)
        if (e.id == id)
            return &e;
    return nullptr;
}

static bool ReadFile(const std::string& path, std::vector<char>& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return !out.empty();
}

// IMG version 2: "VER2", count, then 32-byte entries {offset, size (in 2048-byte sectors), name[24]}
bool CSkinImport::BuildImage(const std::string& gameDir)
{
    std::string folder = gameDir + "CoopAndreas\\skins\\";
    CreateDirectoryA((gameDir + "CoopAndreas").c_str(), nullptr);
    CreateDirectoryA(folder.c_str(), nullptr);

    // NAME.dff + NAME.txd pairs
    std::map<std::string, std::pair<std::string, std::string>> pairs;  // lower name -> (dff path, txd path)
    std::map<std::string, std::string> displayName;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((folder + "*.*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                continue;
            std::string file = fd.cFileName;
            size_t dot = file.rfind('.');
            if (dot == std::string::npos)
                continue;
            std::string base = file.substr(0, dot), ext = Lower(file.substr(dot + 1));
            std::string key = Lower(base);
            if (ext == "dff")
            {
                pairs[key].first = folder + file;
                displayName[key] = base;
            }
            else if (ext == "txd")
                pairs[key].second = folder + file;
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }

    struct Item
    {
        std::string name;
        std::vector<char> data;
    };
    std::vector<Item> items;
    for (auto& [key, files] : pairs)
    {
        if (files.first.empty() || files.second.empty())
        {
            printf("[skins] '%s' ignorada: falta el .%s con el mismo nombre\n", key.c_str(), files.first.empty() ? "dff" : "txd");
            continue;
        }
        if (ms_entries.size() >= 99)
            break;
        char internal[16];
        sprintf_s(internal, "csk%02d", (int)ms_entries.size());
        Item dff{std::string(internal) + ".dff"}, txd{std::string(internal) + ".txd"};
        if (!ReadFile(files.first, dff.data) || !ReadFile(files.second, txd.data))
            continue;
        items.push_back(std::move(dff));
        items.push_back(std::move(txd));
        ms_entries.push_back({displayName[key], internal, SkinIdFor(key)});
    }

    if (items.empty())
        return false;

    const uint32_t SECTOR = 2048;
    uint32_t headerSize = 8 + (uint32_t)items.size() * 32;
    uint32_t offset = (headerSize + SECTOR - 1) / SECTOR;

    std::ofstream img(gameDir + "CoopAndreas\\coop_skins.img", std::ios::binary | std::ios::trunc);
    if (!img)
        return false;
    img.write("VER2", 4);
    uint32_t count = (uint32_t)items.size();
    img.write((const char*)&count, 4);
    std::vector<uint32_t> offsets;
    for (const Item& it : items)
    {
        uint32_t sectors = ((uint32_t)it.data.size() + SECTOR - 1) / SECTOR;
        uint16_t streaming = (uint16_t)sectors, archive = 0;
        char name[24] = {};
        strncpy_s(name, it.name.c_str(), 23);
        img.write((const char*)&offset, 4);
        img.write((const char*)&streaming, 2);
        img.write((const char*)&archive, 2);
        img.write(name, 24);
        offsets.push_back(offset);
        offset += sectors;
    }
    for (size_t i = 0; i < items.size(); i++)
    {
        img.seekp((std::streamoff)offsets[i] * SECTOR);
        img.write(items[i].data.data(), items[i].data.size());
        size_t pad = (SECTOR - items[i].data.size() % SECTOR) % SECTOR;
        if (pad)
        {
            std::vector<char> zeros(pad, 0);
            img.write(zeros.data(), pad);
        }
    }
    return true;
}

bool CSkinImport_BuildImage(const std::string& dir)
{
    return CSkinImport::BuildImageFor(dir);
}

// 0x5B8E1B: CStreaming::Init2 -> CStreaming::LoadCdDirectory(). Our IMG is added to the list right
// before the game reads the directories of all its IMG files.
static uintptr_t s_LoadCdDirectory_orig = 0;
static void __cdecl LoadCdDirectory_Hook()
{
    char exe[MAX_PATH];
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string dir = exe;
    dir = dir.substr(0, dir.rfind('\\') + 1);

    if (CSkinImport::GetAll().empty() && CSkinImport_BuildImage(dir))
    {
        int index = CStreaming::AddImageToList("CoopAndreas\\coop_skins.img", true);
        printf("[skins] %d skin(s) importada(s), IMG #%d\n", (int)CSkinImport::GetAll().size(), index);
    }
    plugin::CallDyn(s_LoadCdDirectory_orig);
}

void CSkinImport::Install()
{
    s_LoadCdDirectory_orig = injector::GetBranchDestination(0x5B8E1B).as_int();
    patch::RedirectCall(0x5B8E1B, (void*)LoadCdDirectory_Hook);
}
