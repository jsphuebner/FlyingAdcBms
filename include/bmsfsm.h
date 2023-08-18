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
#ifndef BMSFSM_H
#define BMSFSM_H
#include "canmap.h"
#include "canhardware.h"
#include "params.h"

class BmsFsm: public CanCallback
{
   public:
      enum bmsstate { BOOT, GET_ADDR, SET_ADDR, INIT, RUN, RUNBALANCE };

      BmsFsm(CanMap* cm);
      bmsstate Run(bmsstate currentState);
      int GetNumberOfModules() { return numModules; }
      Param::PARAM_NUM GetDataItem(Param::PARAM_NUM baseItem, int modNum = -1);
      bool HandleRx(uint32_t canId, uint32_t data[2]);
      void HandleClear();
      bool IsFirst();
      uint8_t GetMaxSubmodules() { return 8; }

   private:
      void MapCanSubmodule();
      void MapCanMainmodule();

      CanMap *canMap;
      bool recvBoot;
      bool isMain;
      uint8_t recvAddr;
      uint8_t ourAddr;
      uint8_t numModules;
      uint32_t cycles;
};

#endif // BMSFSM_H
