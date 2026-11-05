#pragma once
#include <functional>
#include <string>
#include <vector>

// [private build] in-game menu (F2), San Andreas help-box style: cheats (through the cheat sync),
// vehicle spawner, skins, player helpers, teleports, save anywhere.
class CModMenu
{
public:
    struct Item
    {
        std::string label;
        std::function<std::string()> right;   // text on the right side (state), may be empty
        std::function<void()> onSelect;       // nullptr = opens `submenu`
        std::function<void(CModMenu*)> submenu;
    };
    struct Page
    {
        std::string title;
        std::vector<Item> items;
        int selected = 0;
        int scroll = 0;
    };

    static bool IsOpen() { return ms_bOpen; }
    static void Process();  // gameProcessEvent
    static void Draw();     // drawingEvent

    // page builders
    void Open(const std::string& title, std::vector<Item> items);
    void Push(Page page);

private:
    static bool ms_bOpen;
    static std::vector<Page> ms_pages;
    static unsigned int ms_nLastInput;
    static std::string ms_status;
    static unsigned int ms_nStatusUntil;

    static void Toggle();
    static void BuildRoot(CModMenu* m);
    static void BuildCheats(CModMenu* m);
    static void BuildVehicles(CModMenu* m);
    static void BuildVehicleCategory(CModMenu* m, int category);
    static void BuildSkins(CModMenu* m);
    static void BuildPlayer(CModMenu* m);
    static void BuildTeleport(CModMenu* m);
    static void SetStatus(const std::string& text);
};
