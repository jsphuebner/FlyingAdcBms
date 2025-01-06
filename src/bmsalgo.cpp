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
//voltage to state of charge            0%    10%   20%   30%   40%   50%   60%   70%   80%   90%   100%
uint16_t BmsAlgo::voltageToSoc[] =    { 3300, 3400, 3450, 3500, 3560, 3600, 3700, 3800, 4000, 4100, 4200 };
float BmsAlgo::ccCurrent[3];
uint16_t BmsAlgo::cvVoltage[3];

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

float BmsAlgo::GetChargeCurrent(float maxCellVoltage)
{
   float result;

   /* Here we try to mimic VWs charge curve for a warm battery.
    *
    * We run 3 subsequent CC-CV curves
    * 1st starts at cc1Current and aims for cv1Voltage
    * 2nd starts at cc2Current and aims for cv2Voltage
    * 3rd starts at cc3Current and aims for cv3Voltage
    *
    * Low temperature derating is done by scaling down the CC values
    * High temp derating is done by generally capping charge current
    */

   float cv1Result = (cvVoltage[0] - maxCellVoltage) * 3; //P-controller gain factor 3 A/mV
   cv1Result = MIN(cv1Result, ccCurrent[0]);

   float cv2Result = (cvVoltage[1] - maxCellVoltage) * 2;
   cv2Result = MIN(cv2Result, ccCurrent[1]);

   float cv3Result = (cvVoltage[2] - maxCellVoltage) * 2;
   cv3Result = MIN(cv3Result, ccCurrent[2]);
   cv3Result = MAX(cv3Result, 0);

   result = MAX(cv1Result, MAX(cv2Result, cv3Result));

   return result;
}

float BmsAlgo::LimitMinimumCellVoltage(float minVoltage, float limit)
{
   float factor = (minVoltage - limit) / 50; //start limiting 50mV before hitting minimum
   factor = MAX(0, factor);
   factor = MIN(1, factor);
   return factor;
}

float BmsAlgo::LowTemperatureDerating(float lowTemp)
{
   const float drt1Temp = 25.0f;
   const float drt2Temp = 0;
   const float drt3Temp = -20.0f;
   const float factorAtDrt2 = 0.3f;
   float factor;

   //We allow the ideal charge curve above 25°C
   if (lowTemp > drt1Temp)
      factor = 1;
   else if (lowTemp > drt2Temp) //above 0°C allow at least factorAtDrt2 fraction of the charge current and ramp up linearly with temperature
      factor = factorAtDrt2 + (1 - factorAtDrt2) * (lowTemp - drt2Temp) / (drt1Temp - drt2Temp);
   else if (lowTemp > drt3Temp) //below 0°C ramp down linearly
      factor = factorAtDrt2 * (lowTemp - drt3Temp) / (drt2Temp - drt3Temp);
   else
      factor = 0; //inhibit charging below -20°C

   return factor;
}

float BmsAlgo::HighTemperatureDerating(float highTemp, float maxTemp)
{
   float factor = (maxTemp - highTemp) * 0.15f;
   factor = MIN(1, factor);
   factor = MAX(0, factor);

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

/** \brief Sets a charge current curve.
 *
 * The overall charge curve is determined by 3 consecutive CC/CV curves
 * charging starts with CC1 and aims for CV1
 * Once the current drop below the CC value of curve 2 CC/CV curve 2 becomes active
 * Likewise for curve 3
 *
 * \param idx Index of CC/CV curve 0, 1, 2
 * \param current Constant current value
 * \param voltage voltage target
 *
 */
void BmsAlgo::SetCCCVCurve(uint8_t idx, float current, uint16_t voltage)
{
   if (idx > 2) return;
   ccCurrent[idx] = current;
   cvVoltage[idx] = voltage;
}

float BmsAlgo::CalculateSoH(float lastSoc, float newSoc, float asDiff)
{
   float soh = -1;
   float estimatedAs = newSoc - lastSoc; //Calculate difference in percent to last estimation
   estimatedAs = ABS(estimatedAs); //only the absolute value is of interest

   if (estimatedAs > 20) //Only estimate on larger SoC steps
   {
      estimatedAs*= nominalCapacity; //multiply with supposedly available amp hours
      estimatedAs*= 3600.0f / 100; //multiply by 3600 (Ah to As) and divide by 100 because percent
      soh = asDiff / estimatedAs;
      soh *= 100;
   }
   return soh;
}
