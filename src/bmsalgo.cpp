/*
 * This file is part of the stm32-... project.
 *
 * Copyright (C) 2021 Johannes Huebner <dev@johanneshuebner.com>
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
#include "bmsalgo.h"
#include "my_math.h"

float BmsAlgo::nominalCapacity;
//voltage to state of charge                0%    10%   20%   30%   40%   50%   60%   70%   80%   90%   100%
uint16_t BmsAlgo::voltageToSoc[] =       { 3300, 3400, 3450, 3500, 3560, 3600, 3700, 3800, 4000, 4100, 4200 };
//state of charge to normalized charge current
float BmsAlgo::socToChargeCurrent[] = { 1.0f,  1.0f, 1.0f, 1.0f, 1.0f, 0.625f,0.5f,0.4f, 0.3f,0.125f,  0 };

float BmsAlgo::CalculateSocFromIntegration(float lastSoc, float asDiff)
{
   float soc = lastSoc + (100 * asDiff / (3600 * nominalCapacity));
   return soc;
}

float BmsAlgo::EstimateSocFromVoltage(float lowestVoltage)
{
   int n = sizeof(voltageToSoc) / sizeof(voltageToSoc[0]);

   for (int i = 0; i < n; i++)
   {
      if (lowestVoltage < voltageToSoc[i])
      {
         if (i == 0) return 0;

         float soc = i * 10;
         float lutDiff = voltageToSoc[i] - voltageToSoc[i - 1];
         float valDiff = voltageToSoc[i] - lowestVoltage;
         //interpolate
         soc -= (valDiff / lutDiff) * 10;
         return soc;
      }
   }
   return 100;
}

float BmsAlgo::GetChargeCurrent(float soc, float maxCurrent)
{
   if (soc < 0) return socToChargeCurrent[0];
   if (soc > 100) return 0;

   //Interpolate between two normalized currents. Say soc=44%, current@40%=1.0, current@50%=0.625
   //then current = 0.625 + (10 - 44 % 10) * (1 - 0.625) / 10

   int i = soc / 10;
   float lutDiff = socToChargeCurrent[i] - socToChargeCurrent[i + 1];
   float valDiff = 10 - (10 - (soc - i * 10));
   float current = socToChargeCurrent[i] - valDiff * lutDiff / 10;
   current *= maxCurrent;

   return current;
}

float BmsAlgo::LimitMinumumCellVoltage(float minVoltage, float limit)
{
   float factor = (minVoltage - limit) / 50; //start limiting 50mV before hitting minimum
   factor = MAX(0, factor);
   factor = MIN(1, factor);
   return factor;
}

/** \brief Returns a derating factor to stay below maximum cell voltage
 *
 * \param maxVoltage the highest cell voltage of the entire pack
 * \return float derating factor
 *
 */
float BmsAlgo::LimitMaximumCellVoltage(float maxVoltage, float limit)
{
   float factor = (limit - maxVoltage) / 10; //start limiting 10mV before hitting maximum
   factor = MAX(0, factor);
   factor = MIN(1, factor);
   return factor;
}

/** \brief Sets a lookup point for open circuit SoC estimation
 *
 * \param soc soc at multiples of 10, so 0, 10, 20,...,100
 * \param voltage open circuit voltage at that SoC
 *
 */
void BmsAlgo::SetSocLookupPoint(uint8_t soc, uint16_t voltage)
{
   if (soc > 100) return;
   voltageToSoc[soc / 10] = voltage;
}

/** \brief Sets a charge current limit at a given SoC
 *
 * \param soc soc at multiples of 10 up to 90%, so 0, 10, 20,...,90
 * \param factor current derating factor
 *
 */
void BmsAlgo::SetChargeLimit(uint8_t soc, float factor)
{
   if (soc > 90) return;
   socToChargeCurrent[soc / 10] = factor;
}
