/*

Copyright 2015-2021 Igor Petrovic

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

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include "core/src/general/Timing.h"
#include "io/analog/Analog.h"
#include "core/src/general/Helpers.h"

namespace IO
{
    class AnalogFilter : public IO::Analog::Filter
    {
        public:
        AnalogFilter(IO::Analog::adcType_t adcType, size_t stableValueRepetitions)
            : _adcType(adcType)
            , _adcConfig(adcType == IO::Analog::adcType_t::adc10bit ? adc10bit : adc12bit)
            , _stableValueRepetitions(stableValueRepetitions)
            , _stepDiff7Bit(static_cast<uint16_t>(adcType) / 128)
        {}

        Analog::adcType_t adcType() override
        {
            return _adcType;
        }

        bool isFiltered(size_t index, Analog::type_t type, uint16_t value, uint16_t& filteredValue) override
        {
            auto compare = [](const void* a, const void* b) {
                if (*(uint16_t*)a < *(uint16_t*)b)
                    return -1;
                else if (*(uint16_t*)a > *(uint16_t*)b)
                    return 1;

                return 0;
            };

            value = CONSTRAIN(value, 0, _adcConfig.adcMaxValue);

            //avoid filtering in this case for faster response
            if (type == Analog::type_t::button)
            {
                if (value < _adcConfig.digitalValueThresholdOff)
                    filteredValue = 1;
                else if (value > _adcConfig.digitalValueThresholdOn)
                    filteredValue = 0;
                else
                    filteredValue = 0;

                return true;
            }

            bool fastFilter = true;

            if (index < MAX_NUMBER_OF_ANALOG)
            {
                //don't filter the readings for touchscreen data

                if ((core::timing::currentRunTimeMs() - _lastMovementTime[index]) > FAST_FILTER_ENABLE_AFTER_MS)
                {
                    fastFilter                                    = false;
                    _analogSample[index][_sampleCounter[index]++] = value;

                    //take the median value to avoid using outliers
                    if (_sampleCounter[index] == 3)
                    {
                        qsort(_analogSample[index], 3, sizeof(uint16_t), compare);
                        _sampleCounter[index] = 0;
                        filteredValue         = _analogSample[index][1];
                    }
                    else
                    {
                        return false;
                    }
                }
                else
                {
                    filteredValue = value;
                }
            }
            else
            {
                filteredValue = value;
            }

            bool     use14bit = false;
            uint16_t maxLimit;
            uint16_t stepDiff;

            if ((type == Analog::type_t::nrpn14b) || (type == Analog::type_t::pitchBend) || (type == Analog::type_t::cc14bit))
                use14bit = true;

            if (use14bit)
            {
                maxLimit = MIDI_14_BIT_VALUE_MAX;
                stepDiff = _adcConfig.stepDiff14Bit;
            }
            else
            {
                maxLimit = MIDI_7_BIT_VALUE_MAX;
                stepDiff = _stepDiff7Bit;
            }

            //if the first read value is 0, mark it as increasing
            bool direction = filteredValue >= _lastStableValue[index];

            //always use 7bit diff value here regardless of the type of value (7bit/14bit)
            //for 14bit value this will improve stability since those values are scaled anyways and use small diff value
            if (direction != _lastDirection[index])
                stepDiff = _stepDiff7Bit;

            if (abs(filteredValue - _lastStableValue[index]) < stepDiff)
            {
                _stableSampleCount[index] = 0;
                return false;
            }

            auto midiValue    = core::misc::mapRange(static_cast<uint32_t>(filteredValue), static_cast<uint32_t>(0), static_cast<uint32_t>(_adcConfig.adcMaxValue), static_cast<uint32_t>(0), static_cast<uint32_t>(maxLimit));
            auto oldMIDIvalue = core::misc::mapRange(static_cast<uint32_t>(_lastStableValue[index]), static_cast<uint32_t>(0), static_cast<uint32_t>(_adcConfig.adcMaxValue), static_cast<uint32_t>(0), static_cast<uint32_t>(maxLimit));

            if (midiValue == oldMIDIvalue)
            {
                _stableSampleCount[index] = 0;
                return false;
            }

            auto acceptNewValue = [&]() {
                _stableSampleCount[index] = 0;
                _lastDirection[index]     = direction;
                _lastStableValue[index]   = filteredValue;
                _sampleCounter[index]     = 0;
                _lastMovementTime[index]  = core::timing::currentRunTimeMs();
            };

            if (fastFilter)
            {
                acceptNewValue();
            }
            else
            {
                if (++_stableSampleCount[index] >= _stableValueRepetitions)
                    acceptNewValue();
                else
                    return false;
            }

            if (type == Analog::type_t::fsr)
            {
                filteredValue = core::misc::mapRange(CONSTRAIN(filteredValue, static_cast<uint32_t>(_adcConfig.fsrMinValue), static_cast<uint32_t>(_adcConfig.fsrMaxValue)), static_cast<uint32_t>(_adcConfig.fsrMinValue), static_cast<uint32_t>(_adcConfig.fsrMaxValue), static_cast<uint32_t>(0), static_cast<uint32_t>(MIDI_7_BIT_VALUE_MAX));
            }
            else
            {
                filteredValue = midiValue;
            }

            //when edge values are reached, disable fast filter by resetting last movement time
            if ((midiValue == 0) || (midiValue == maxLimit))
                _lastMovementTime[index] = 0;

            return true;
        }

        void reset(size_t index) override
        {
            _sampleCounter[index] = 0;
        }

        private:
        using adcConfig_t = struct
        {
            const uint16_t adcMaxValue;                 ///< Maxmimum raw ADC value.
            const uint16_t stepDiff14Bit;               ///< Minimum difference between two raw ADC readings to consider that value has been changed for 14-bit MIDI values.
            const uint16_t fsrMinValue;                 ///< Minimum raw ADC reading for FSR sensors.
            const uint16_t fsrMaxValue;                 ///< Maximum raw ADC reading for FSR sensors.
            const uint16_t aftertouchMaxValue;          ///< Maxmimum raw ADC reading for aftertouch on FSR sensors.
            const uint16_t digitalValueThresholdOn;     ///< Value above which buton connected to analog input is considered pressed.
            const uint16_t digitalValueThresholdOff;    ///< Value below which button connected to analog input is considered released.
        };

        adcConfig_t adc10bit = {
            .adcMaxValue              = 1000,
            .stepDiff14Bit            = 1,
            .fsrMinValue              = 40,
            .fsrMaxValue              = 340,
            .aftertouchMaxValue       = 600,
            .digitalValueThresholdOn  = 1000,
            .digitalValueThresholdOff = 600,
        };

        adcConfig_t adc12bit = {
            .adcMaxValue              = 4000,
            .stepDiff14Bit            = 2,
            .fsrMinValue              = 160,
            .fsrMaxValue              = 1360,
            .aftertouchMaxValue       = 2400,
            .digitalValueThresholdOn  = 4000,
            .digitalValueThresholdOff = 2400,
        };

        const IO::Analog::adcType_t _adcType;
        adcConfig_t&                _adcConfig;
        const size_t                _stableValueRepetitions;
        const uint16_t              _stepDiff7Bit;

        static constexpr uint32_t FAST_FILTER_ENABLE_AFTER_MS                                                     = 500;
        uint16_t                  _analogSample[MAX_NUMBER_OF_ANALOG + MAX_NUMBER_OF_TOUCHSCREEN_COMPONENTS][3]   = {};
        size_t                    _sampleCounter[MAX_NUMBER_OF_ANALOG + MAX_NUMBER_OF_TOUCHSCREEN_COMPONENTS]     = {};
        bool                      _lastDirection[MAX_NUMBER_OF_ANALOG + MAX_NUMBER_OF_TOUCHSCREEN_COMPONENTS]     = {};
        uint16_t                  _lastStableValue[MAX_NUMBER_OF_ANALOG + MAX_NUMBER_OF_TOUCHSCREEN_COMPONENTS]   = {};
        uint8_t                   _stableSampleCount[MAX_NUMBER_OF_ANALOG + MAX_NUMBER_OF_TOUCHSCREEN_COMPONENTS] = {};
        uint32_t                  _lastMovementTime[MAX_NUMBER_OF_ANALOG + MAX_NUMBER_OF_TOUCHSCREEN_COMPONENTS]  = {};
    };    // namespace IO
}    // namespace IO