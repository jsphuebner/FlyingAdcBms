/*
 * This file is part of the FlyingAdcBms project.
 *
 * Copyright (C) 2023 Johannes Huebner <openinverter.org>
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
#ifndef FLYINGADCBMS_H
#define FLYINGADCBMS_H


class FlyingAdcBms
{
   public:
      enum BalanceCommand { BAL_OFF, BAL_DISCHARGE, BAL_CHARGE };
      enum BalanceStatus  { STT_OFF, STT_DISCHARGE, STT_CHARGEPOS, STT_CHARGENEG };

      static void Init();
      static void MuxOff();
      static void SelectChannel(uint8_t channel);
      static void StartAdc();
      static float GetResult(float gain);
      static BalanceStatus SetBalancing(BalanceCommand cmd);

   protected:

   private:
      static uint8_t selectedChannel, previousChannel;
};

#endif // FLYINGADCBMS_H
