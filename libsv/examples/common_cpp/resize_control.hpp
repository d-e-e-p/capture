#pragma once

#include "sv/sv.h"

namespace common 
{
    enum ResizeValue { None = 0, FullHd, Hd, Vga };

    class ResizeControl : public IControl {

        public:
            ResizeControl();
            ~ResizeControl();
            uint32_t GetID() override;
            const char* GetName() override;
            int32_t Get() override;
            bool Set(int32_t val) override;
            int32_t GetMinValue() override;
            int32_t GetMaxValue() override;
            int32_t GetStepValue() override;
            int32_t GetDefaultValue() override;
            MenuEntryList GetMenuEntries() override;
            bool IsMenu() override;
        
        private:
            int32_t value;
            int32_t min;
            int32_t max;
            int32_t defaultValue;
            MenuEntryList entries;
    };
}