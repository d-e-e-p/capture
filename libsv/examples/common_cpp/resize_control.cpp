#include "resize_control.hpp"
#include "frame_size.hpp"
#include <stdexcept>
#include <sstream>
#include <iostream>

namespace common
{

ResizeControl::ResizeControl()
{
    entries.push_back(MenuEntry{"disabled", ResizeValue::None});
    entries.push_back(MenuEntry{"1920 x 1080", ResizeValue::FullHd});
    entries.push_back(MenuEntry{"1280 x 720", ResizeValue::Hd});
    entries.push_back(MenuEntry{"640 x 480", ResizeValue::Vga});

    defaultValue = ResizeValue::None;

    min = 0;
    max = entries.size() - 1;
    value = defaultValue;
}

ResizeControl::~ResizeControl()
{

}
            
uint32_t ResizeControl::GetID()
{
    throw std::invalid_argument("Custom control does not have an ID");
}

const char* ResizeControl::GetName()
{
    return "Resizing";
}

int32_t ResizeControl::Get()
{
    return value;
}

bool ResizeControl::Set(int32_t val) 
{
    if (val < min || val > max) { 
        return false;
    }
    value = val;
    return true;
}

int32_t ResizeControl::GetMinValue()
{
    return min;
}

int32_t ResizeControl::GetMaxValue()
{
    return max;
}
            
int32_t ResizeControl::GetStepValue()
{
    return 1;
}
            
int32_t ResizeControl::GetDefaultValue()
{
    return defaultValue;
}

MenuEntryList ResizeControl::GetMenuEntries()
{
    return entries;
}

bool ResizeControl::IsMenu() 
{
    return true;
}


}