#pragma once

#include "sv/sv.h"

namespace common 
{
    class DebayeringControl : public IControl {

        public:
            DebayeringControl();
            ~DebayeringControl();
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
            MenuEntryList entries;
    };
}

