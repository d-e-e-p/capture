#pragma once

#include <vector>
#include <functional>

namespace common
{
    enum SpecialHotkey { ENTER = 13, ESC = 27 };

    class HotkeyAction
    {
        public:
            HotkeyAction(std::vector<char> hotkeys, std::string description, std::function<void()> action);
            ~HotkeyAction();
            std::string GetDescription() const;
            bool IsValidHotkey(char hotkey) const;
            void Perform() const;
        
        private:
            std::string description;
            std::vector<char> hotkeys;
            std::function<void()> action;
            std::string ParseHotkey(char hotkey);
    };
}