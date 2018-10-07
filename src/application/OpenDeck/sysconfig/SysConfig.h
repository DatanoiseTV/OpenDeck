/*
    OpenDeck MIDI platform firmware
    Copyright (C) 2015-2018 Igor Petrovic

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "sysex/src/SysEx.h"
#include "CustomIDs.h"
#include "blocks/Blocks.h"
#include "board/Board.h"
#include "database/Database.h"
#include "midi/src/MIDI.h"
#include "interface/digital/input/buttons/Buttons.h"
#include "interface/digital/input/encoders/Encoders.h"
#include "interface/analog/Analog.h"
#ifdef LEDS_SUPPORTED
#include "interface/digital/output/leds/LEDs.h"
#endif

class SysConfig : public SysEx
{
    public:
    #ifdef LEDS_SUPPORTED
    #ifdef DISPLAY_SUPPORTED
    SysConfig(Board &board, Database &database, MIDI &midi, Buttons &buttons, Encoders &encoders, Analog &analog, LEDs &leds, Display &display) :
    #else
    SysConfig(Board &board, Database &database, MIDI &midi, Buttons &buttons, Encoders &encoders, Analog &analog, LEDs &leds) :
    #endif
    #else
    #ifdef DISPLAY_SUPPORTED
    SysConfig(Board &board, Database &database, MIDI &midi, Buttons &buttons, Encoders &encoders, Analog &analog, Display &display) :
    #else
    SysConfig(Board &board, Database &database, MIDI &midi, Buttons &buttons, Encoders &encoders, Analog &analog) :
    #endif
    #endif
    board(board),
    database(database),
    midi(midi),
    buttons(buttons),
    encoders(encoders),
    analog(analog)
    #ifdef LEDS_SUPPORTED
    ,leds(leds)
    #endif
    #ifdef DISPLAY_SUPPORTED
    ,display(display)
    #endif
    {}

    void init();

    bool isProcessingEnabled();

    ///
    /// \brief Configures UART read/write handlers for MIDI module.
    /// @param [in] channel     UART channel on MCU.
    ///
    void setupMIDIoverUART(uint8_t channel);

    ///
    /// \brief Configures OpenDeck UART MIDI format on specified UART channel.
    /// OpenDeck UART MIDI format is a specially formatted MIDI message which
    /// is sent over UART and uses USB MIDI packet format with byte value
    /// OD_FORMAT_MIDI_DATA_START prepended before the message start and XOR of
    /// 4 USB packet bytes as the last byte. XOR byte is used for error checking.
    /// This is used to avoid parsing of MIDI messages when using boards such as Arduino
    /// Mega or Arduino Uno which feature two separate MCUs - the main one and the other for
    /// USB communication. Using this format USB MCU can quickly forward the received message
    /// from UART to USB interface - first byte (OD_FORMAT_MIDI_DATA_START) and last byte
    /// (XOR value) are removed from the output.
    /// @param [in] channel     UART channel on MCU.
    ///
    void setupMIDIoverUART_OD(uint8_t channel);

    bool sendCInfo(dbBlockID_t dbBlock, sysExParameter_t componentID);

    private:
    Board       &board;
    Database    &database;
    MIDI        &midi;
    Buttons     &buttons;
    Encoders    &encoders;
    Analog      &analog;
    #ifdef LEDS_SUPPORTED
    LEDs        &leds;
    #endif
    #ifdef DISPLAY_SUPPORTED
    Display     &display;
    #endif

    ///
    /// Used to prevent updating states of all components (analog, LEDs, encoders, buttons).
    ///
    bool     processingEnabled;

    bool onGet(uint8_t block, uint8_t section, uint16_t index, sysExParameter_t &value);
    bool onSet(uint8_t block, uint8_t section, uint16_t index, sysExParameter_t newValue);
    bool onCustomRequest(uint8_t value);
    void onWrite(uint8_t *sysExArray, uint8_t size);

    uint32_t lastCinfoMsgTime[DB_BLOCKS];
};