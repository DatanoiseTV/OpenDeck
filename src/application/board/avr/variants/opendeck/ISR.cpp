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
#include "../../../../interface/digital/output/leds/Variables.h"
#include "../../../../interface/digital/output/leds/Helpers.h"
#include "pins/Map.h"
#include "board/common/constants/LEDs.h"
#include "board/common/digital/input/matrix/Variables.h"
#include "board/common/analog/input/Variables.h"
#include "board/common/digital/output/matrix/Variables.h"
#include "board/common/indicators/Variables.h"

///
/// \brief Implementation of core variable used to keep track of run time in milliseconds.
///
volatile uint32_t rTime_ms;

volatile bool MIDIreceived;
volatile bool MIDIsent;

///
/// \brief Variables used to control the time MIDI in/out LED indicators on board are active.
/// When these LEDs need to be turned on, variables are set to value representing time in
/// milliseconds during which they should be on. ISR decreases variable value by 1 every 1 millsecond.
/// Once the variables have value 0, specific LED indicator is turned off.
/// @{

static uint8_t midiIn_timeout;
static uint8_t midiOut_timeout;

/// @}

void Board::configureTimers()
{
    //clear timer0 conf
    TCCR0A = 0;
    TCCR0B = 0;
    TIMSK0 = 0;

    //clear timer1 conf
    TCCR1A = 0;
    TCCR1B = 0;

    //clear timer3 conf
    TCCR3A = 0;
    TCCR3B = 0;

    //clear timer4 conf
    TCCR4A = 0;
    TCCR4B = 0;
    TCCR4C = 0;
    TCCR4D = 0;
    TCCR4E = 0;

    //set timer1, timer3 and timer4 to phase correct pwm mode
    //timer 1
    TCCR1A |= (1<<WGM10);           //phase correct PWM
    TCCR1B |= (1<<CS10);            //prescaler 1
    //timer 3
    TCCR3A |= (1<<WGM30);           //phase correct PWM
    TCCR3B |= (1<<CS30);            //prescaler 1
    //timer 4
    TCCR4A |= (1<<PWM4A);           //Pulse Width Modulator A Enable
    TCCR4B |= (1<<CS40);            //prescaler 1
    TCCR4C |= (1<<PWM4D);           //Pulse Width Modulator D Enable
    TCCR4D |= (1<<WGM40);           //phase correct PWM

    //set timer0 to ctc, used for millis/led matrix
    TCCR0A |= (1<<WGM01);           //CTC mode
    TCCR0B |= (1<<CS01)|(1<<CS00);  //prescaler 64
    OCR0A = 62;                     //250us
    TIMSK0 |= (1<<OCIE0A);          //compare match interrupt
}

///
/// \brief Activates currently active button matrix column (stored in activeInColumn variable).
///
inline void activateInputColumn()
{
    BIT_READ(dmColumnArray[activeInColumn], 0) ? setHigh(DEC_DM_A0_PORT, DEC_DM_A0_PIN) : setLow(DEC_DM_A0_PORT, DEC_DM_A0_PIN);
    BIT_READ(dmColumnArray[activeInColumn], 1) ? setHigh(DEC_DM_A1_PORT, DEC_DM_A1_PIN) : setLow(DEC_DM_A1_PORT, DEC_DM_A1_PIN);
    BIT_READ(dmColumnArray[activeInColumn], 2) ? setHigh(DEC_DM_A2_PORT, DEC_DM_A2_PIN) : setLow(DEC_DM_A2_PORT, DEC_DM_A2_PIN);
}

///
/// \brief Activates currently active LED matrix column (stored in activeOutColumn variable).
///
inline void activateOutputColumn()
{
    BIT_READ(activeOutColumn, 0) ? setHigh(DEC_LM_A0_PORT, DEC_LM_A0_PIN) : setLow(DEC_LM_A0_PORT, DEC_LM_A0_PIN);
    BIT_READ(activeOutColumn, 1) ? setHigh(DEC_LM_A1_PORT, DEC_LM_A1_PIN) : setLow(DEC_LM_A1_PORT, DEC_LM_A1_PIN);
    BIT_READ(activeOutColumn, 2) ? setHigh(DEC_LM_A2_PORT, DEC_LM_A2_PIN) : setLow(DEC_LM_A2_PORT, DEC_LM_A2_PIN);
}

///
/// Acquires data for all buttons connected in currently active button matrix column by
/// reading inputs from shift register.
///
inline void storeDigitalIn()
{
    setLow(SR_CLK_PORT, SR_CLK_PIN);
    setLow(SR_LATCH_PORT, SR_LATCH_PIN);
    _NOP();

    digitalInBuffer[activeInColumn] = 0;

    setHigh(SR_LATCH_PORT, SR_LATCH_PIN);

    for (int i=0; i<8; i++)
    {
        setLow(SR_CLK_PORT, SR_CLK_PIN);
        _NOP();
        BIT_WRITE(digitalInBuffer[activeInColumn], dmRowBitArray[i], !readPin(SR_DIN_PORT, SR_DIN_PIN));
        setHigh(SR_CLK_PORT, SR_CLK_PIN);
    }
}

///
/// \brief Turns all rows in LED matrix off.
///
inline void ledRowsOff()
{
    //turn off PWM
    TCCR1A &= ~(1<<COM1C1);
    TCCR4C &= ~(1<<COM4D1);
    TCCR1A &= ~(1<<COM1A1);
    TCCR4A &= ~(1<<COM4A1);
    TCCR3A &= ~(1<<COM3A1);
    TCCR1A &= ~(1<<COM1B1);

    for (int i=0; i<NUMBER_OF_LED_ROWS; i++)
        EXT_LED_OFF(*(ledRowPortArray[i]), ledRowPinArray[i]);
}

///
/// \brief Turns requested LED row on with specified PWM intensity.
/// @param [in] rowNumber   Row in LED matrix which is being turned on.
/// @param [in] intensity   PWM intensity of requested row.
///
inline void ledRowOn(uint8_t rowNumber, uint8_t intensity)
{
    if (intensity == 255)
    {
        //max value, don't use pwm
        EXT_LED_ON(*(ledRowPortArray[rowNumber]), ledRowPinArray[rowNumber]);
    }
    else
    {
        #ifdef LED_EXT_INVERT
        intensity = 255 - intensity;
        #endif

        switch (rowNumber)
        {
            //turn off pwm if intensity is max
            case 0:
            OCR1C = intensity;
            TCCR1A |= (1<<COM1C1);
            break;

            case 1:
            OCR4D = intensity;
            TCCR4C |= (1<<COM4D1);
            break;

            case 2:
            OCR1A = intensity;
            TCCR1A |= (1<<COM1A1);
            break;

            case 3:
            OCR4A = intensity;
            TCCR4A |= (1<<COM4A1);
            break;

            case 4:
            OCR3A = intensity;
            TCCR3A |= (1<<COM3A1);
            break;

            case 5:
            OCR1B = intensity;
            TCCR1A |= (1<<COM1B1);
            break;

            default:
            break;
        }
    }
}

///
/// \brief Checks if any of LEDs in currently active LED matrix column needs to be turned on.
/// This function also performs transition effect betweeen two LED states.
///
inline void checkLEDs()
{
    //if there is an active LED in current column, turn on LED row
    //do fancy transitions here
    for (int i=0; i<NUMBER_OF_LED_ROWS; i++)
    {
        uint8_t ledNumber = activeOutColumn+i*NUMBER_OF_LED_COLUMNS;
        uint8_t ledStateSingle = LED_ON(ledState[ledNumber]);

        ledStateSingle *= (NUMBER_OF_LED_TRANSITIONS-1);

        //don't bother with pwm if it's disabled
        if (!pwmSteps && ledStateSingle)
        {
            ledRowOn(i, 255);
        }
        else
        {
            if (ledTransitionScale[transitionCounter[ledNumber]])
                ledRowOn(i, ledTransitionScale[transitionCounter[ledNumber]]);

            if (transitionCounter[ledNumber] != ledStateSingle)
            {
                //fade up
                if (transitionCounter[ledNumber] < ledStateSingle)
                {
                    transitionCounter[ledNumber] += pwmSteps;

                    if (transitionCounter[ledNumber] > ledStateSingle)
                        transitionCounter[ledNumber] = ledStateSingle;
                }
                else
                {
                    //fade down
                    transitionCounter[ledNumber] -= pwmSteps;

                    if (transitionCounter[ledNumber] < 0)
                        transitionCounter[ledNumber] = 0;
                }
            }
        }
    }
}

///
/// \brief Main interrupt service routine.
/// Used to control LED and button matrix and to update current run time.
///
ISR(TIMER0_COMPA_vect)
{
    //update blink every 1ms
    //update led matrix every 1ms
    //update button matrix each time

    static uint8_t updateStuff = 0;
    updateStuff++;

    if (analogSampleCounter != NUMBER_OF_ANALOG_SAMPLES)
        startADCconversion();

    if (updateStuff == 4)
    {
        ledRowsOff();

        if (activeOutColumn == NUMBER_OF_LED_COLUMNS)
            activeOutColumn = 0;

        activateOutputColumn();
        checkLEDs();

        activeOutColumn++;
        rTime_ms++;

        //read input matrix
        if (activeInColumn < NUMBER_OF_BUTTON_COLUMNS)
        {
            for (int i=0; i<NUMBER_OF_BUTTON_COLUMNS; i++)
            {
                activeInColumn = i;
                activateInputColumn();
                storeDigitalIn();
            }

            activeInColumn = NUMBER_OF_BUTTON_COLUMNS;
        }

        if (MIDIreceived)
        {
            INT_LED_ON(LED_IN_PORT, LED_IN_PIN);
            MIDIreceived = false;
            midiIn_timeout = MIDI_INDICATOR_TIMEOUT;
        }

        if (MIDIsent)
        {
            INT_LED_ON(LED_OUT_PORT, LED_OUT_PIN);
            MIDIsent = false;
            midiOut_timeout = MIDI_INDICATOR_TIMEOUT;
        }

        if (midiIn_timeout)
            midiIn_timeout--;
        else
            INT_LED_OFF(LED_IN_PORT, LED_IN_PIN);

        if (midiOut_timeout)
            midiOut_timeout--;
        else
            INT_LED_OFF(LED_OUT_PORT, LED_OUT_PIN);

        updateStuff = 0;
    }
}

///
/// \brief Configures one of 16 inputs/outputs on 4067 multiplexer.
///
inline void setMuxInput()
{
    BIT_READ(muxPinOrderArray[activeMuxInput], 0) ? setHigh(MUX_S0_PORT, MUX_S0_PIN) : setLow(MUX_S0_PORT, MUX_S0_PIN);
    BIT_READ(muxPinOrderArray[activeMuxInput], 1) ? setHigh(MUX_S1_PORT, MUX_S1_PIN) : setLow(MUX_S1_PORT, MUX_S1_PIN);
    BIT_READ(muxPinOrderArray[activeMuxInput], 2) ? setHigh(MUX_S2_PORT, MUX_S2_PIN) : setLow(MUX_S2_PORT, MUX_S2_PIN);
    BIT_READ(muxPinOrderArray[activeMuxInput], 3) ? setHigh(MUX_S3_PORT, MUX_S3_PIN) : setLow(MUX_S3_PORT, MUX_S3_PIN);
}

///
/// \brief ADC ISR used to read values from multiplexers.
///
ISR(ADC_vect)
{
    analogBuffer[analogIndex] += ADC;
    analogIndex++;
    activeMuxInput++;

    bool switchMux = (activeMuxInput == NUMBER_OF_MUX_INPUTS);

    if (switchMux)
    {
        activeMuxInput = 0;
        activeMux++;

        if (activeMux == NUMBER_OF_MUX)
        {
            activeMux = 0;
            analogIndex = 0;
            analogSampleCounter++;
        }

        setADCchannel(muxInPinArray[activeMux]);
    }

    //always set mux input
    setMuxInput();
}
