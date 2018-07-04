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

#include "Variables.h"
#include "Hardware.h"
#include "Pins.h"
#include "board/common/constants/LEDs.h"
#include "board/common/indicators/Variables.h"

volatile uint32_t rTime_ms;
volatile uint8_t midiIn_timeout, midiOut_timeout;

volatile bool MIDIreceived, MIDIsent;

ISR(TIMER0_COMPA_vect)
{
    if (MIDIreceived)
    {
        MIDI_LED_ON(LED_IN_PORT, LED_IN_PIN);
        MIDIreceived = false;
        midiIn_timeout = MIDI_INDICATOR_TIMEOUT;
    }

    if (MIDIsent)
    {
        MIDI_LED_ON(LED_OUT_PORT, LED_OUT_PIN);
        MIDIsent = false;
        midiOut_timeout = MIDI_INDICATOR_TIMEOUT;
    }

    if (midiIn_timeout)
        midiIn_timeout--;
    else
        MIDI_LED_OFF(LED_IN_PORT, LED_IN_PIN);

    if (midiOut_timeout)
        midiOut_timeout--;
    else
        MIDI_LED_OFF(LED_OUT_PORT, LED_OUT_PIN);

    rTime_ms++;
}