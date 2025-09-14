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
#include "selftest.h"

#define IS_FIRST_THRESH       1800
#define IS_ENABLED_THRESH     500
#define SDO_INDEX_PARAMS      0x2000
#define BOOT_DELAY_CYCLES     5

BmsFsm::BmsFsm(CanMap* cm, CanSdo* cs)
   : canMap(cm), canSdo(cs), isMain(false), infoIndex(1), numModules(1), cycles(0)
{
   cm->GetHardware()->AddCallback(this);
   HandleClear();
   recvNodeId = Param::GetInt(Param::sdobase);
   pdobase = Param::GetInt(Param::pdobase);
   ourNodeId = recvNodeId;
   ourIndex = 0;
   recvIndex = 0;
}

/**
 * @brief Executes the state machine for the Battery Management System (BMS).
 *
 * This function processes the current state of the BMS finite state machine (FSM)
 * and transitions to the next state based on the defined logic. The function handles
 * various states including initialization, address setting, information request,
 * and operational states. It also manages transitions based on conditions such as
 * node identification, self-test results, and current measurements.
 *
 * The states handled by this function include:
 * - BOOT: Initializes the system as main or sub module.
 * - GET_ADDR: Retrieves the address of the node.
 * - SET_ADDR: Sets the address of the next node and prepares for information requests.
 * - REQ_INFO: Requests information from the sub modules.
 * - RECV_INFO: Receives and processes information from sub modules.
 * - INIT: Initializes the BMS hardware.
 * - SELFTEST: Performs self-tests and checks results.
 * - RUN: Operates the BMS and monitors current.
 * - IDLE: Enters idle mode after a period of inactivity.
 * - ERROR: Handles error states and checks for re-enabling.
 *
 * @param currentState The current state of the BMS FSM, represented as a
 *                     BmsFsm::bmsstate enum value.
 *
 * @return The next state of the BMS FSM, represented as a
 *         BmsFsm::bmsstate enum value, based on the state transition logic.
 */
BmsFsm::bmsstate BmsFsm::Run(bmsstate currentState)
{
   uint32_t data[2] = { 0 };
   uint32_t sdoReply;

   switch (currentState)
   {
   case BOOT:
      if (IsFirst())
      {
         cycles = 0;
         recvNodeId = Param::GetInt(Param::sdobase);
         pdobase = Param::GetInt(Param::pdobase);
         canSdo->SetNodeId(recvNodeId);
         canMap->Clear();
         DigIo::nextena_out.Set();
         isMain = true;
         MapCanMainmodule();
         Param::SetInt(Param::totalcells, Param::GetInt(Param::numchan));
         return SET_ADDR;
      }
      else
      {
         recvNodeId = 0;
         return GET_ADDR;
      }
      break;
   case GET_ADDR:
      if (recvNodeId > 0)
      {
         ourNodeId = recvNodeId;
         ourIndex = recvIndex;
         pdobase = recvPdoBase;
         canSdo->SetNodeId(ourNodeId);
         DigIo::nextena_out.Set();
         canMap->Clear();
         MapCanSubmodule();
         Param::SetInt(Param::modaddr, ourNodeId);
         return SET_ADDR;
      }
      break;
   case SET_ADDR:
      cycles++;

      if (cycles == BOOT_DELAY_CYCLES)
      {
         data[1] = recvNodeId + 1;
         data[1] |= (ourIndex + 1) << 8;
         data[1] |= pdobase << 16;
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
      return SELFTEST;
   case SELFTEST:
      if (SelfTest::GetLastResult() == SelfTest::TestsDone)
         return RUN;
      if (SelfTest::GetLastResult() == SelfTest::TestFailed)
      {
         Param::SetInt(Param::enable, 0);
         return ERROR;
      }
      break;
   case RUN:
      if (!IsEnabled() && !IsFirst())
      {
         //sub modules turn off when main module turns off
         DigIo::selfena_out.Clear();
         DigIo::nextena_out.Clear();
      }

      if (ABS(Param::GetFloat(Param::idcavg)) < Param::GetFloat(Param::idlethresh))
      {
         cycles++;

         if (cycles > ((uint32_t)Param::GetInt(Param::idlewait) * 10))
         {
            cycles = 0;
            return IDLE;
         }
      }
      else
      {
         cycles = 0;
      }
      break;
   case IDLE:
      cycles++;

      if (ABS(Param::GetFloat(Param::idcavg)) > Param::GetFloat(Param::idlethresh))
      {
         return RUN;
      }

      if (!IsEnabled() && !IsFirst())
      {
         //sub modules turn off when main module turns off
         DigIo::selfena_out.Clear();
         DigIo::nextena_out.Clear();
      }

      if (cycles > (uint32_t)Param::GetInt(Param::turnoffwait) && !IsEnabled())
      {
         DigIo::selfena_out.Clear();
         DigIo::nextena_out.Clear();
      }
      break;
   case ERROR:
      if (Param::GetBool(Param::enable))
         return RUN;
      break;
   case REBOOT:
      if (DigIo::nextena_out.Get())
         cycles = 0;

      DigIo::nextena_out.Clear();
      cycles++;

      if (cycles == 10)
      {
         return BOOT;
      }
      break;
   }
   return currentState;
}

Param::PARAM_NUM BmsFsm::GetDataItem(Param::PARAM_NUM baseItem, int modNum)
{
   const int numberOfParametersPerModule = 5;
   if (modNum < 0) modNum = ourIndex;

   return (Param::PARAM_NUM)((int)baseItem + modNum * numberOfParametersPerModule);
}

void BmsFsm::HandleClear()
{
   canMap->GetHardware()->RegisterUserMessage(0x7dd);
}

void BmsFsm::HandleRx(uint32_t canId, uint32_t data[2], uint8_t)
{
   switch (canId)
   {
   case 0x7dd:
      recvNodeId = data[1] & 0xFF;
      recvIndex = (data[1] >> 8) & 0xFF;
      recvPdoBase = data[1] >> 16;
   }
}

bool BmsFsm::IsFirst()
{
   int enableLevel = AnaIn::enalevel.Get();

   return isMain || enableLevel > IS_FIRST_THRESH;
}

bool BmsFsm::IsEnabled()
{
   int enableLevel = AnaIn::enalevel.Get();

   return enableLevel > IS_ENABLED_THRESH;
}

void BmsFsm::MapCanSubmodule()
{
   int id = pdobase + ourIndex + 1; //main module has two PDO messages
   canMap->AddSend(Param::umin0, id, 0, 14, 1);
   canMap->AddSend(Param::umax0, id, 16, 14, 1);
   canMap->AddSend(Param::counter, id, 30, 2, 1);
   canMap->AddSend(Param::uavg0, id, 32, 14, 1);
   canMap->AddSend(Param::tempmin0, id, 48, 8, 1);
   canMap->AddSend(Param::tempmax0, id, 56, 8, 1);

   canMap->AddRecv(Param::idcavg, pdobase, 32, 16, 0.1);
   canMap->AddRecv(Param::umin, pdobase + 1, 0, 14, 1);
   canMap->AddRecv(Param::umax, pdobase + 1, 16, 14, 1);
   canMap->AddRecv(Param::uavg, pdobase + 1, 32, 14, 1);
}

void BmsFsm::MapCanMainmodule()
{
   for (int i = 1; i < GetMaxSubmodules(); i++)
   {
      int id = pdobase + i + 1;
      canMap->AddRecv(GetDataItem(Param::umin0, i), id, 0, 14, 1);
      canMap->AddRecv(GetDataItem(Param::umax0, i), id, 16, 14, 1);
      canMap->AddRecv(GetDataItem(Param::uavg0, i), id, 32, 14, 1);
      canMap->AddRecv(GetDataItem(Param::tempmin0, i), id, 48, 8, 1);
      canMap->AddRecv(GetDataItem(Param::tempmax0, i), id, 56, 8, 1);
   }

   int id = Param::GetInt(Param::pdobase);

   //we don't expose our local accumulated values but the "global" ones
   canMap->AddSend(Param::umin, id + 1, 0, 14, 1);
   canMap->AddSend(Param::umax, id + 1, 16, 14, 1);
   canMap->AddSend(Param::counter, id + 1, 30, 2, 1);
   canMap->AddSend(Param::uavg, id + 1, 32, 14, 1);
   canMap->AddSend(Param::tempmin, id + 1, 48, 8, 1);
   canMap->AddSend(Param::tempmax, id + 1, 56, 8, 1);

   canMap->AddSend(Param::chargelim, id, 0, 11, 1);
   canMap->AddSend(Param::dischargelim, id, 11, 11, 1);
   canMap->AddSend(Param::soc, id, 22, 10, 10);
   canMap->AddSend(Param::idcavg, id, 32, 16, 10);
   canMap->AddSend(Param::utotal, id, 48, 10, 0.001f);
   canMap->AddSend(Param::counter, id, 62, 2, 1);
}
