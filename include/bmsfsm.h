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
#include "cansdo.h"
#include "params.h"

#define MAX_SUB_MODULES 8

class BmsFsm: public CanCallback
{
   public:
      enum bmsstate { BOOT, GET_ADDR, SET_ADDR, REQ_INFO, RECV_INFO, INIT, SELFTEST, RUN, IDLE, ERROR };

      BmsFsm(CanMap* cm, CanSdo* cs);
      bmsstate Run(bmsstate currentState);
      int GetNumberOfModules() { return numModules; }
      uint8_t GetCellsOfModule(uint8_t mod) { return numChan[mod]; }
      Param::PARAM_NUM GetDataItem(Param::PARAM_NUM baseItem, int modNum = -1);
      void HandleRx(uint32_t canId, uint32_t data[2], uint8_t);
      void HandleClear();
      bool IsFirst();
      bool IsEnabled();
      uint8_t GetMaxSubmodules() { return MAX_SUB_MODULES; }

   private:
      void MapCanSubmodule();
      void MapCanMainmodule();

      CanMap *canMap;
      CanSdo *canSdo;
      bool isMain;
      uint8_t recvNodeId;
      uint8_t recvIndex;
      uint16_t recvPdoBase;
      uint8_t ourNodeId;
      uint8_t ourIndex;
      uint16_t pdobase;
      uint8_t infoIndex;
      uint8_t numModules;
      uint32_t cycles;
      uint8_t numChan[MAX_SUB_MODULES + 1]; //sub modules plus one master module
};

#endif // BMSFSM_H
