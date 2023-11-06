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
#include "bmsfsm.h"
#include "anain.h"
#include "digio.h"
#include "my_math.h"
#include "flyingadcbms.h"

#define IS_FIRST_THRESH       1800
#define SDO_INDEX_PARAMS      0x2000
#define MAIN_MSG_ID           0x12C

BmsFsm::BmsFsm(CanMap* cm, CanSdo* cs)
   : canMap(cm), canSdo(cs), isMain(false), infoIndex(1), numModules(1), cycles(0)
{
   cm->GetHardware()->AddCallback(this);
   HandleClear();
   recvAddr = Param::GetInt(Param::sdobase);
   ourAddr = recvAddr;
}

BmsFsm::bmsstate BmsFsm::Run(bmsstate currentState)
{
   uint32_t data[2] = { 0 };
   uint32_t sdoReply;

   switch (currentState)
   {
   case BOOT:
      if (IsFirst())
      {
         canSdo->SetNodeId(recvAddr);
         canMap->Clear();
         DigIo::nextena_out.Set();
         isMain = true;
         MapCanMainmodule();
         Param::SetInt(Param::totalcells, Param::GetInt(Param::numchan));
         return SET_ADDR;
      }
      else
      {
         recvAddr = 0;
         return GET_ADDR;
      }
      break;
   case GET_ADDR:
      if (recvAddr > 0)
      {
         ourAddr = recvAddr;
         canSdo->SetNodeId(ourAddr);
         DigIo::nextena_out.Set();
         canMap->Clear();
         MapCanSubmodule();
         return SET_ADDR;
      }
      break;
   case SET_ADDR:
      cycles++;

      if (cycles == 5)
      {
         data[1] = recvAddr + 1;
         canMap->GetHardware()->Send(0x7dd, data);
         cycles = 0;
         return isMain ? REQ_INFO : INIT;
      }
      break;
   case REQ_INFO:
      cycles++;

      if (cycles == 10)
      {
         canSdo->SDORead(infoIndex + Param::GetInt(Param::sdobase), SDO_INDEX_PARAMS, (uint8_t)Param::numchan);

         return RECV_INFO;
      }
      break;
   case RECV_INFO:
      if (canSdo->SDOReadReply(sdoReply))
      {
         numChan[infoIndex] = FP_TOINT(sdoReply); //numbers are transmitted in 5 bit fixed point
         Param::SetInt(Param::totalcells, Param::GetInt(Param::totalcells) + numChan[infoIndex]);
         numModules++;
         Param::SetInt(Param::modnum, numModules);
         infoIndex++;
         cycles = 0;
         return infoIndex < MAX_SUB_MODULES ? REQ_INFO : INIT;
      }
      return INIT;
      break;
   case INIT:
      FlyingAdcBms::Init();
      return RUN;
   case RUN:
      if (ABS(Param::GetFloat(Param::idcavg)) < 0.8f)
      {
         cycles++;

         if (cycles > ((uint32_t)Param::GetInt(Param::idlewait) * 10))
         {
            cycles = 0;
            return RUNBALANCE;
         }
      }
      else
      {
         cycles = 0;
      }
      break;
   case RUNBALANCE:
      cycles++;

      if (ABS(Param::GetFloat(Param::idcavg)) > 0.8f)
      {
         return RUN;
      }

      //After 2 hours turn off
      if (cycles > 72000)
      {
         DigIo::selfena_out.Clear();
      }
      break;
   }
   return currentState;
}

Param::PARAM_NUM BmsFsm::GetDataItem(Param::PARAM_NUM baseItem, int modNum)
{
   const int numberOfParametersPerModule = 4;
   if (modNum < 0) modNum = ourAddr - Param::GetInt(Param::sdobase);

   return (Param::PARAM_NUM)((int)baseItem + modNum * numberOfParametersPerModule);
}

void BmsFsm::HandleClear()
{
   canMap->GetHardware()->RegisterUserMessage(0x7dd);
}

bool BmsFsm::HandleRx(uint32_t canId, uint32_t data[2])
{
   switch (canId)
   {
   case 0x7dd:
      recvAddr = data[1] & 0xFF;
      return true;
   }
   return false;
}

bool BmsFsm::IsFirst()
{
   int enableLevel = AnaIn::enalevel.Get();

   return isMain || enableLevel > IS_FIRST_THRESH;
}

void BmsFsm::MapCanSubmodule()
{
   int id = Param::GetInt(Param::pdobase) + ourAddr - Param::GetInt(Param::sdobase);
   canMap->AddSend(Param::umin0, id, 0, 13, 1);
   canMap->AddSend(Param::umax0, id, 16, 13, 1);
   canMap->AddSend(Param::counter, id, 30, 2, 1);
   canMap->AddSend(Param::uavg0, id, 32, 13, 1);
   canMap->AddSend(Param::temp0, id, 48, 8, 1, 40); //TODO: send here tempmin and tempmax
   canMap->AddSend(Param::temp0, id, 56, 8, 1, 40);

   canMap->AddRecv(Param::idc, MAIN_MSG_ID, 32, 16, 1); //will convert to signed in code
   canMap->AddRecv(Param::umin, Param::GetInt(Param::pdobase), 0, 13, 1);
   canMap->AddRecv(Param::umax, Param::GetInt(Param::pdobase), 16, 13, 1);
   canMap->AddRecv(Param::uavg, Param::GetInt(Param::pdobase), 32, 13, 1);
}

void BmsFsm::MapCanMainmodule()
{
   for (int i = 1; i < GetMaxSubmodules(); i++)
   {
      int id = Param::GetInt(Param::pdobase) + i;
      canMap->AddRecv(GetDataItem(Param::umin0, i), id, 0, 13, 1);
      canMap->AddRecv(GetDataItem(Param::umax0, i), id, 16, 13, 1);
      canMap->AddRecv(GetDataItem(Param::uavg0, i), id, 32, 13, 1);
      canMap->AddRecv(GetDataItem(Param::temp0, i), id, 48, 8, 1, -40);
   }

   int id = Param::GetInt(Param::pdobase);

   //we don't expose our local accumulated values but the "global" ones
   canMap->AddSend(Param::umin, id, 0, 13, 1);
   canMap->AddSend(Param::umax, id, 16, 13, 1);
   canMap->AddSend(Param::counter, id, 30, 2, 1);
   canMap->AddSend(Param::uavg, id, 32, 13, 1);
   canMap->AddSend(Param::tempmin, id, 48, 8, 1, 40);
   canMap->AddSend(Param::tempmax, id, 56, 8, 1, 40);

   canMap->AddSend(Param::chargelim, MAIN_MSG_ID, 0, 11, 1);
   canMap->AddSend(Param::dischargelim, MAIN_MSG_ID, 11, 11, 1);
   canMap->AddSend(Param::soc, MAIN_MSG_ID, 22, 10, 10);
   canMap->AddSend(Param::idcavg, MAIN_MSG_ID, 32, 16, 10);
   canMap->AddSend(Param::utotal, MAIN_MSG_ID, 48, 10, 0.001f);
   canMap->AddSend(Param::counter, MAIN_MSG_ID, 62, 2, 1);
}
