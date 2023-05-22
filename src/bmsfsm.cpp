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

#define IS_FIRST_THRESH 1800

BmsFsm::BmsFsm(CanMap* cm)
   : canMap(cm), recvBoot(false), isMain(false), cycles(0)
{
   HandleClear();
   recvAddr = Param::GetInt(Param::sdobase);
   ourAddr = recvAddr;
}

BmsFsm::bmsstate BmsFsm::Run(bmsstate currentState)
{
   uint32_t data[2] = { 0 };

   switch (currentState)
   {
   case BOOT:
      if (IsFirst())
      {
         canMap->SetNodeId(recvAddr);
         DigIo::nextena_out.Set();
         canMap->Clear();
         isMain = true;
         MapCanMainmodule();
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
         canMap->SetNodeId(recvAddr);
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
         return INIT;
      }
      break;
   case INIT:
      FlyingAdcBms::Init();
      return RUN;
   case RUN:
      if (IsFirst() && ABS(Param::GetFloat(Param::idcavg)) < 0.8f)
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

      if (cycles > ((uint32_t)Param::GetInt(Param::idlewait) * 20))
      {
         DigIo::selfena_out.Clear();
      }
      break;
   }
   return currentState;
}

Param::PARAM_NUM BmsFsm::GetDataItem(Param::PARAM_NUM baseItem, int modNum)
{
   const int numberOfParametersPerModule = 3;
   if (modNum < 0) modNum = ourAddr - Param::GetInt(Param::sdobase);

   return (Param::PARAM_NUM)((int)baseItem + modNum * numberOfParametersPerModule);
}

void BmsFsm::HandleClear()
{
   canMap->GetHardware()->RegisterUserMessage(0x7dd);
   canMap->GetHardware()->RegisterUserMessage(0x7de);
}

bool BmsFsm::HandleRx(uint32_t canId, uint32_t data[2])
{
   switch (canId)
   {
   case 0x7dd:
      recvAddr = data[1] & 0xFF;
      //Whenever we receive an address it means the number of modules increased
      //subtract the base address to arrive at the actual number of modules
      numModules = recvAddr - Param::GetInt(Param::sdobase);
      Param::SetInt(Param::modnum, numModules);
      break;
   case 0x7de:
      recvBoot = true;
      break;
   default:
      return false;
   }
   return true;
}

bool BmsFsm::IsFirst()
{
   int enableLevel = AnaIn::enalevel.Get();

   return isMain || enableLevel > IS_FIRST_THRESH;
}

void BmsFsm::MapCanSubmodule()
{
   int id = Param::GetInt(Param::pdobase) + ourAddr - Param::GetInt(Param::sdobase);
   canMap->AddSend(Param::umin, id, 0, 16, 1);
   canMap->AddSend(Param::umax, id, 16, 16, 1);
   canMap->AddSend(Param::uavg, id, 32, 16, 1);
   canMap->AddSend(Param::numchan, id, 48, 8, 1);
   canMap->AddSend(Param::temp, id, 56, 8, 1, 40);
}

void BmsFsm::MapCanMainmodule()
{
   for (int i = 1; i < 6; i++)
   {
      int id = Param::GetInt(Param::pdobase) + i;
      canMap->AddRecv(GetDataItem(Param::umin0, i), id, 0, 16, 1);
      canMap->AddRecv(GetDataItem(Param::umax0, i), id, 16, 16, 1);
      canMap->AddRecv(GetDataItem(Param::uavg0, i), id, 32, 16, 1);
   }

   MapCanSubmodule(); //we're doing these as well, just they're accumulated from all modules

   canMap->AddSend(Param::chargelim, 300, 0, 10, 1);
   canMap->AddSend(Param::dischargelim, 300, 10, 10, 1);
   canMap->AddSend(Param::soc, 300, 20, 10, 4);
   canMap->AddSend(Param::idcavg, 300, 40, 12, 1);
   canMap->AddSend(Param::utotal, 300, 52, 10, 0.001);
}
