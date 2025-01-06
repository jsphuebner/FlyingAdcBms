/*
 * This file is part of the stm32-sine project.
 *
 * Copyright (C) 2010 Johannes Huebner <contact@johanneshuebner.com>
 * Copyright (C) 2010 Edward Cheeseman <cheesemanedward@gmail.com>
 * Copyright (C) 2009 Uwe Hermann <uwe@hermann-uwe.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "temp_meas.h"
#include <stdint.h>
#include <math.h>


float TempMeas::AdcToTemperature(int digit, int nomRes, int beta)
{
    /* Convert the resistance to a temperature */
    /* Based on: https://learn.adafruit.com/thermistor/using-a-thermistor */
   const int seriesResistor = 1200;
   const int nominalTemp = 25;
   const float maxAdcValue = 4095.0f;
   const float absoluteZero = 273.15f;
   const float voltageRatio = 5.0f / 3.3f; //Ratio of pull-up voltage and ADC reference voltage
   float resistance = seriesResistor * (voltageRatio / (digit / maxAdcValue) - 1.0f);
   float steinhart = logf(resistance / nomRes) / beta;     // log(R/Ro)/B
   steinhart += 1.0f / (nominalTemp + absoluteZero); // + (1/To)
   steinhart = 1.0f / steinhart;
   steinhart -= absoluteZero;

   return steinhart;
}
