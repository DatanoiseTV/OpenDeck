/*

Copyright 2015-2020 Igor Petrovic

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/

#include "board/Board.h"
#include "board/Internal.h"
#include "EmuEEPROM/src/EmuEEPROM.h"
#include <vector>
#include "core/src/general/Atomic.h"
#include "core/src/general/Timing.h"
#include "board/common/constants/IO.h"

namespace
{
    class STM32F4EEPROM : public EmuEEPROM::StorageAccess
    {
        public:
        STM32F4EEPROM() = default;

        bool init() override
        {
            uint32_t totalStorageSpace = Board::detail::map::flashPageDescriptor(Board::detail::map::eepromFlashPage1()).size / 4 - 1;
            eepromMemory.resize(totalStorageSpace, 0xFFFF);

            return true;
        }

        uint32_t startAddress(EmuEEPROM::page_t page) override
        {
            switch (page)
            {
            case EmuEEPROM::page_t::pageFactory:
                return Board::detail::map::flashPageDescriptor(Board::detail::map::eepromFlashPageFactory()).address;

            case EmuEEPROM::page_t::page2:
                return Board::detail::map::flashPageDescriptor(Board::detail::map::eepromFlashPage2()).address;

            default:
                return Board::detail::map::flashPageDescriptor(Board::detail::map::eepromFlashPage1()).address;
            }
        }

        bool erasePage(EmuEEPROM::page_t page) override
        {
            switch (page)
            {
            case EmuEEPROM::page_t::pageFactory:
                return Board::detail::flash::erasePage(Board::detail::map::eepromFlashPageFactory());

            case EmuEEPROM::page_t::page2:
                return Board::detail::flash::erasePage(Board::detail::map::eepromFlashPage2());

            default:
                return Board::detail::flash::erasePage(Board::detail::map::eepromFlashPage1());
            }
        }

        bool write16(uint32_t address, uint16_t data) override
        {
            return Board::detail::flash::write16(address, data);
        }

        bool write32(uint32_t address, uint32_t data) override
        {
            return Board::detail::flash::write32(address, data);
        }

        bool read16(uint32_t address, uint16_t& data) override
        {
            return Board::detail::flash::read16(address, data);
        }

        bool read32(uint32_t address, uint32_t& data) override
        {
            return Board::detail::flash::read32(address, data);
        }

        uint32_t pageSize() override
        {
            return Board::detail::map::flashPageDescriptor(Board::detail::map::eepromFlashPage1()).size;
        }

        ///
        /// \brief Memory array stored in RAM holding all the values stored in virtual EEPROM.
        /// Used to avoid constant lookups in the flash.
        ///
        std::vector<uint16_t> eepromMemory;
    };

    STM32F4EEPROM stm32EEPROM;
    EmuEEPROM     emuEEPROM(stm32EEPROM, true);
}    // namespace

namespace Board
{
    namespace NVM
    {
        bool init()
        {
            bool result;

            //disable all interrupts during init to avoid ISRs messing up the flash page formatting/setup
            ATOMIC_SECTION
            {
                result = emuEEPROM.init();
            }

            return result;
        }

        uint32_t size()
        {
            return stm32EEPROM.pageSize();
        }

        bool read(uint32_t address, int32_t& value, parameterType_t type)
        {
            uint16_t tempData;

            switch (type)
            {
            case parameterType_t::byte:
            case parameterType_t::word:
                if (stm32EEPROM.eepromMemory[address] != 0xFFFF)
                {
                    value = stm32EEPROM.eepromMemory[address];
                }
                else
                {
                    if (emuEEPROM.read(address, tempData) != EmuEEPROM::readStatus_t::ok)
                    {
                        return false;
                    }
                    else
                    {
                        value                             = tempData;
                        stm32EEPROM.eepromMemory[address] = tempData;
                    }
                }
                break;

            default:
                return false;
                break;
            }

            return true;
        }

        bool write(uint32_t address, int32_t value, parameterType_t type)
        {
            uint16_t tempData;

            switch (type)
            {
            case parameterType_t::byte:
            case parameterType_t::word:
                tempData                          = value;
                stm32EEPROM.eepromMemory[address] = value;
                if (emuEEPROM.write(address, tempData) != EmuEEPROM::writeStatus_t::ok)
                    return false;
                break;

            default:
                return false;
                break;
            }

            return true;
        }

        bool clear(uint32_t start, uint32_t end)
        {
            bool result;

            //clearing is usually called in runtime so it's possible that LED
            //indicators are still on since the command is most likely given via USB
            //wait until all indicators are turned off
            core::timing::waitMs(MIDI_INDICATOR_TIMEOUT);

            ATOMIC_SECTION
            {
                result = emuEEPROM.format();
            }

            //ignore start/end markers on stm32 for now
            return result;
        }

        size_t paramUsage(parameterType_t type)
        {
            switch (type)
            {
            case parameterType_t::dword:
                return 8;

            default:
                return 4;    //2 bytes for address, 2 bytes for data
            }
        }
    }    // namespace NVM
}    // namespace Board