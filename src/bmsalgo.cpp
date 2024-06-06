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
   const float lowTempDerate = 1; //LowTempDerating();
   const float highTempDerate = 1; //HighTempDerating(50);
   const float cc1Current = ccCurrent[0] * lowTempDerate;
   const uint16_t cv1Voltage = cvVoltage[0];
   const float cc2Current = ccCurrent[1] * lowTempDerate;
   const uint16_t cv2Voltage = cvVoltage[1];
   const float cc3Current = ccCurrent[2] * lowTempDerate;
   const uint16_t cv3Voltage = cvVoltage[2];
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

   float cv1Result = (cv1Voltage - maxCellVoltage) * 3; //P-controller gain factor 3 A/mV
   cv1Result = MIN(cv1Result, cc1Current);

   float cv2Result = (cv2Voltage - maxCellVoltage) * 2;
   cv2Result = MIN(cv2Result, cc2Current);

   float cv3Result = (cv3Voltage - maxCellVoltage) * 2;
   cv3Result = MIN(cv3Result, cc3Current);
   cv3Result = MAX(cv3Result, 0);

   result = MAX(cv1Result, MAX(cv2Result, cv3Result));
   result *= highTempDerate;

   return result;
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
