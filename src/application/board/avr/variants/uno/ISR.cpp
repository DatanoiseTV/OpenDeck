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

#include "board/Board.h"
#include "pins/Map.h"
#include "board/common/analog/input/Variables.h"
#include "board/common/digital/input/Variables.h"
#include "board/common/digital/input/direct/Variables.h"
#include "board/common/digital/input/Variables.h"
#include "../../../../interface/digital/output/leds/Variables.h"
#include "../../../../interface/digital/output/leds/Helpers.h"
#include "board/common/indicators/Variables.h"
#include "board/common/constants/LEDs.h"

volatile uint32_t rTime_ms;

static uint8_t lastLEDstate[MAX_NUMBER_OF_LEDS];

inline void storeDigitalIn()
{
    uint8_t data = 0;

    //clear old data
    digitalInBuffer[digitalInBufferCounter] = 0;

    for (int i=0; i<MAX_NUMBER_OF_BUTTONS; i++)
    {
        data <<= 1;
        data |= !readPin(*(dInPortArray[i]), dInPinArray[i]); //invert reading because of pull-up configuration
    }

    digitalInBuffer[digitalInBufferCounter] = data;
    digitalInBufferCounter++;
}

inline void checkLEDs()
{
    //if there is an active LED in current column, turn on LED row
    //do fancy transitions here
    for (int i=0; i<MAX_NUMBER_OF_LEDS; i++)
    {
        uint8_t ledStateSingle = LED_ON(ledState[i]);

        if (ledStateSingle != lastLEDstate[i])
        {
            if (ledStateSingle)
            {
                EXT_LED_ON(*(dOutPortArray[i]), dOutPinArray[i]);
            }
            else
            {
                EXT_LED_OFF(*(dOutPortArray[i]), dOutPinArray[i]);
            }

            lastLEDstate[i] = ledStateSingle;
        }
    }
}

ISR(TIMER0_COMPA_vect)
{
    //update buttons and leds every 1ms
    //update run timer every 1ms
    //use this timer to start adc conversion every 250us

    static uint8_t updateStuff = 0;
    updateStuff++;

    if (analogSampleCounter != NUMBER_OF_ANALOG_SAMPLES)
        startADCconversion();

    if (updateStuff == 4)
    {
        checkLEDs();
        rTime_ms++;
        updateStuff = 0;

        if (digitalInBufferCounter < DIGITAL_IN_BUFFER_SIZE)
            storeDigitalIn();
    }
}

ISR(ADC_vect)
{
    analogBuffer[analogIndex] += ADC;
    analogIndex++;

    if (analogIndex == MAX_NUMBER_OF_ANALOG)
    {
        analogIndex = 0;
        analogSampleCounter++;
    }

    //always set mux input
    setADCchannel(aInPinMap[analogIndex]);
}