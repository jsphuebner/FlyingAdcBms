/*
 * This file is part of the tumanako_vc project.
 *
 * Copyright (C) 2018 Johannes Huebner <dev@johanneshuebner.com>
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
#ifndef BMSALGO_H
#define BMSALGO_H

#include <stdint.h>
#include "picontroller.h"

class BmsAlgo
{
   public:
      static float EstimateSocFromVoltage(float lowestVoltage);
      static float CalculateSocFromIntegration(float lastSoc, float asDiff);
      static float CalculateSoH(float lastSoc, float newSoc, float asDiff);
      static float GetChargeCurrent(float maxCellVoltage, float hystVoltage, float icutoff);
      static float LimitMinimumCellVoltage(float minVoltage, float limit);
      static float LowTemperatureDerating(float lowTemp);
      static float HighTemperatureDerating(float highTemp, float maxTemp);
      static void SetNominalCapacity(float c) { nominalCapacity = c; }
      static void SetSocLookupPoint(uint8_t soc, uint16_t voltage);
      static void SetCCCVCurve(uint8_t idx, float current, uint16_t voltage);

   private:
      static float nominalCapacity;
      static uint16_t voltageToSoc[11];
      static PiController cvControllers[3]; //Support 3 consecutive CC/CV curves
      static bool full;
};

#endif // BMSALGO_H
