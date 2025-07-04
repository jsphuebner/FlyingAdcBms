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
PiController BmsAlgo::cvControllers[3];
bool BmsAlgo::full;

/** \brief Calculates SoC from a starting point adding the charge through the battery
 *
 * \param lastSoc The last absolute estimated SoC
 * \param asDiff The change in charge i.e. integrated current since estimation
 * \return float the current SoC
 *
 */
float BmsAlgo::CalculateSocFromIntegration(float lastSoc, float asDiff)
{
   float soc = lastSoc + (100 * asDiff / (3600 * nominalCapacity));
   soc = MAX(0, soc);
   soc = MIN(102, soc); //allow slight overrun that doesn't wrap around 0 in CAN message
   return soc;
}

/**
 * @brief Estimates the State of Charge (SoC) of a battery based on the lowest voltage reading.
 *
 * This function uses a lookup table to estimate the State of Charge (SoC) of a battery
 * from a given lowest voltage value. It iterates through the predefined voltage-to-SoC
 * mapping and performs linear interpolation to provide a more accurate SoC estimate
 * when the lowest voltage falls between two known voltage values.
 *
 * @param lowestVoltage The lowest voltage reading from the battery, expressed as a float.
 *
 * @return The estimated State of Charge (SoC) of the battery as a float.
 *         Returns 0 if the lowest voltage is below the first entry in the lookup table,
 *         and returns 100 if the lowest voltage exceeds the last entry.
 */
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

/**
 * @brief Calculates the charge current for a battery based on the maximum cell voltage.
 *
 * This function determines the appropriate charge current for a battery by evaluating
 * the difference between predefined voltage levels and the maximum cell voltage.
 * It implements a three-stage Constant Current-Constant Voltage (CC-CV) charging
 * strategy, where each stage has its own target voltage and corresponding maximum
 * charge current.
 *
 * The function computes the charge current for three stages:
 * - The first stage uses a gain factor to calculate the charge current based on the
 *   difference between the first target voltage and the maximum cell voltage.
 * - The second stage calculates the charge current similarly, using the second target
 *   voltage.
 * - The third stage follows the same approach with the third target voltage.
 *
 * The results from each stage are capped to ensure they do not exceed the defined
 * maximum charge currents and are non-negative. The final charge current is determined
 * by taking the maximum value from the three stages.
 *
 * @param maxCellVoltage The maximum voltage of the battery cell, expressed as a float.
 *
 * @return The calculated charge current for the battery as a float.
 *         The result is capped to ensure it does not exceed the defined current limits
 *         and is non-negative.
 */
float BmsAlgo::GetChargeCurrent(float maxCellVoltage, float hystVoltage, float icutoff)
{
   float result;

   /* Here we try to mimic VWs charge curve for a warm battery.
    *
    * We run 3 subsequent CC-CV curves
    * 1st starts at cc1Current and aims for cv1Voltage
    * 2nd starts at cc2Current and aims for cv2Voltage
    * 3rd starts at cc3Current and aims for cv3Voltage
    *
    */

   float cv1Result = cvControllers[0].Run(FP_FROMFLT(maxCellVoltage / 10));
   float cv2Result = cvControllers[1].Run(FP_FROMFLT(maxCellVoltage / 10));
   float cv3Result = cvControllers[2].Run(FP_FROMFLT(maxCellVoltage / 10));

   result = MAX(cv1Result, MAX(cv2Result, cv3Result));

   if (result < icutoff)
      full = true;

   if (maxCellVoltage < hystVoltage)
      full = false;

   return full ? 0 : result;
}

/**
 * @brief Calculates a limiting factor based on the minimum cell voltage and a specified limit.
 *
 * This function determines a limiting factor that scales linearly based on the difference
 * between the provided minimum voltage and a specified limit. The limiting factor is
 * designed to start limiting when the minimum voltage is 50 mV above the specified limit.
 * The resulting factor is constrained to a range between 0 and 1.
 *
 * @param minVoltage The minimum voltage of the cell, expressed as a float.
 * @param limit The voltage limit to compare against, expressed as a float.
 *
 * @return A float representing the limiting factor, which will be in the range [0, 1].
 *         A factor of 0 indicates no limitation, while a factor of 1 indicates full
 *         limitation based on the minimum voltage.
 */
float BmsAlgo::LimitMinimumCellVoltage(float minVoltage, float limit)
{
   float factor = (minVoltage - limit) / 50; //start limiting 50mV before hitting minimum
   factor = MAX(0, factor);
   factor = MIN(1, factor);
   return factor;
}

/**
 * @brief Calculates the derating factor for battery charging based on low temperature.
 *
 * This function determines a derating factor for battery charging based on the
 * provided low temperature. The derating is applied to ensure safe charging
 * conditions at low temperatures. The function defines specific temperature thresholds
 * to adjust the charging factor:
 * - Above 25°C, the ideal charge curve is allowed (factor = 1).
 * - Between 0°C and 25°C, the factor increases linearly from a minimum value
 *   (factorAtDrt2 = 0.3) to 1 as the temperature rises.
 * - Between -20°C and 0°C, the factor decreases linearly from the minimum value
 *   to 0.
 * - Below -20°C, charging is inhibited (factor = 0).
 *
 * @param lowTemp The low temperature value, expressed as a float.
 *
 * @return A float representing the derating factor for charging, which will be in
 *         the range [0, 1]. A factor of 0 indicates that charging is inhibited,
 *         while a factor of 1 indicates no derating.
 */
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

/**
 * @brief Calculates the derating factor for battery current based on high temperature.
 *
 * This function determines a derating factor for battery current based on the
 * provided high temperature and a maximum temperature limit. The derating factor
 * is calculated to ensure safe operating conditions at high temperatures. The
 * factor is derived from the difference between the maximum temperature and the
 * current high temperature, scaled by a factor of 0.15.
 *
 * The resulting factor is constrained to be within the range [0, 1].
 *
 * @param highTemp The high temperature value, expressed as a float.
 * @param maxTemp The maximum allowable temperature, expressed as a float.
 *
 * @return A float representing the derating factor for charging, which will be in
 *         the range [0, 1]. A factor of 0 indicates full derating, while a factor
 *         of 1 indicates no derating.
 */
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

   cvControllers[idx].SetGains(1, 1);
   cvControllers[idx].SetCallingFrequency(10);
   cvControllers[idx].SetRef(FP_FROMINT(voltage) / 10);
   cvControllers[idx].SetMinMaxY(0, current);
   cvControllers[idx].ResetIntegrator();
}

/**
 * @brief Calculates the State of Health (SoH) of a battery based on the
 *        last and new State of Charge (SoC) values and the actual difference.
 *
 * This function estimates the State of Health (SoH) of a battery by comparing
 * the last and new State of Charge (SoC) values. It calculates the difference
 * in SoC and, if the difference is significant (greater than 20%), it uses
 * this information to estimate the available amp hours and compute the SoH
 * as a percentage.
 *
 * @param lastSoc The last known State of Charge (SoC) of the battery, expressed as a float.
 * @param newSoc The new State of Charge (SoC) of the battery, expressed as a float.
 * @param asDiff The actual difference in amp hours available, expressed as a float.
 *
 * @return The estimated State of Health (SoH) of the battery as a percentage.
 *         Returns -1 if the SoC difference is not significant enough to estimate SoH.
 */
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
