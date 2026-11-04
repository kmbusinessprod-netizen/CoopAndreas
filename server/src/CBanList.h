#pragma once
#include <string>
#include <vector>

// [private build] bans.txt next to server.exe, one nickname per line (case-insensitive).
// TODO: ban by a per-install id once the handshake carries one; names are easy to change.
class CBanList
{
public:
    static void Load();
    static bool IsBanned(const char* name);
    static void Add(const char* name);

private:
    static std::vector<std::string> ms_names;
};
