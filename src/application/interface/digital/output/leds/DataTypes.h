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

typedef enum
{
    rgb_R,
    rgb_G,
    rgb_B
} rgbIndex_t;

typedef enum
{
    colorOff,
    colorRed,
    colorGreen,
    colorYellow,
    colorBlue,
    colorMagenta,
    colorCyan,
    colorWhite,
    LED_COLORS
} ledColor_t;

typedef enum
{
    ledHwParameter_reserved_1,
    ledHwParameterFadeTime,
    ledHwParameterStartUpRoutine,
    LED_HARDWARE_PARAMETERS
} ledHardwareParameter_t;

typedef enum
{
    ledControlMIDIin_noteCC,
    ledControlLocal_Note,
    ledControlMIDIin_CCnote,
    ledControlLocal_CC,
    ledControlMIDIin_PC,
    ledControlLocal_PC,
    LED_CONTROL_TYPES
} ledControlType_t;