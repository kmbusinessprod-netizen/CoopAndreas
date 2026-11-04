#include "stdafx.h"
#include "CBanList.h"
#include <algorithm>
#include <cctype>

std::vector<std::string> CBanList::ms_names;

static std::string Lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' '))
        s.pop_back();
    return s;
}

void CBanList::Load()
{
    ms_names.clear();
    std::ifstream file("bans.txt");
    std::string line;
    while (std::getline(file, line))
    {
        line = Lower(line);
        if (!line.empty())
            ms_names.push_back(line);
    }
    if (!ms_names.empty())
        printf("[Admin] : %d nombre(s) baneado(s) en bans.txt\n", (int)ms_names.size());
}

bool CBanList::IsBanned(const char* name)
{
    std::string n = Lower(name);
    return std::find(ms_names.begin(), ms_names.end(), n) != ms_names.end();
}

void CBanList::Add(const char* name)
{
    if (IsBanned(name))
        return;
    ms_names.push_back(Lower(name));
    std::ofstream file("bans.txt", std::ios::app);
    file << name << "\n";
}
