/*
 * This file is part of the stm32-template project.
 *
 * Copyright (C) 2020 Johannes Huebner <dev@johanneshuebner.com>
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
#include <stdint.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/rtc.h>
#include <libopencm3/stm32/can.h>
#include <libopencm3/stm32/iwdg.h>
#include "stm32_can.h"
#include "canmap.h"
#include "terminal.h"
#include "params.h"
#include "hwdefs.h"
#include "digio.h"
#include "hwinit.h"
#include "anain.h"
#include "param_save.h"
#include "my_math.h"
#include "errormessage.h"
#include "printf.h"
#include "stm32scheduler.h"
#include "terminalcommands.h"
#include "flyingadcbms.h"
#include "bmsfsm.h"
#include "bmsalgo.h"
#include "temp_meas.h"

#define PRINT_JSON 0

static Stm32Scheduler* scheduler;
static CanMap* canMap;
static BmsFsm* bmsFsm;
static bool currentSensorOn = false;

static void Accumulate(float sum, float min, float max, float avg)
{
   if (bmsFsm->IsFirst())
   {
      Param::SetFloat(Param::uavg0, avg);
      Param::SetFloat(Param::umin0, min);
      Param::SetFloat(Param::umax0, max);

      float totalSum = sum, totalMin = min, totalMax = max;
      //If we are the first module accumulate our values with those from the sub modules
      for (int i = 1; i < bmsFsm->GetNumberOfModules(); i++)
      {
         totalSum += Param::GetFloat(bmsFsm->GetDataItem(Param::uavg0, i)) * 16; //TODO use actual number of cells per module
         totalMin = MIN(totalMin, Param::GetFloat(bmsFsm->GetDataItem(Param::umin0, i)));
         totalMax = MIN(totalMax, Param::GetFloat(bmsFsm->GetDataItem(Param::umax0, i)));
      }
      Param::SetFloat(Param::umin, totalMin);
      Param::SetFloat(Param::umax, totalMax);
      Param::SetFloat(Param::uavg, totalSum / (bmsFsm->GetNumberOfModules() * 16));
      Param::SetFloat(Param::udelta, totalMax - totalMin);
      Param::SetFloat(Param::utotal, totalSum);
   }
   else //if we are a sub module write averages straight to data module
   {
      Param::SetFloat(Param::utotal, sum);
      Param::SetFloat(Param::uavg, avg);
      Param::SetFloat(Param::umin, min);
      Param::SetFloat(Param::umax, max);
      Param::SetFloat(Param::udelta, max - min);
   }
}

static void CalculateCurrentLimits()
{
   float chargeCurrentLimit = BmsAlgo::GetChargeCurrent(Param::GetFloat(Param::soc));
   chargeCurrentLimit *= BmsAlgo::LimitMaximumCellVoltage(Param::GetFloat(Param::umax));
   Param::SetFloat(Param::chargelim, chargeCurrentLimit);

   float dischargeCurrentLimit = 500;
   dischargeCurrentLimit *= BmsAlgo::LimitMinumumCellVoltage(Param::GetFloat(Param::umin));
   Param::SetFloat(Param::dischargelim, dischargeCurrentLimit);
}

static void ReadAdc()
{
   int totalBalanceCycles = 30;
   static uint8_t chan = 0, balanceCycles = 0;
   static float sum = 0, min, max, avg;
   bool balance = Param::GetBool(Param::balance);
   FlyingAdcBms::BalanceStatus bstt;

   if (balance)
   {
      if (balanceCycles == 0)
      {
         balanceCycles = totalBalanceCycles;
      }
      else
      {
         balanceCycles--;
      }

      if (balanceCycles > 0 && balanceCycles < (totalBalanceCycles - 1))
      {
         float udc = Param::GetFloat((Param::PARAM_NUM)(Param::u0 + chan));

         if (udc < (avg - 3))
         {
            bstt = FlyingAdcBms::SetBalancing(FlyingAdcBms::BAL_CHARGE);
         }
         else if (udc > (avg + 1))
         {
            bstt = FlyingAdcBms::SetBalancing(FlyingAdcBms::BAL_DISCHARGE);
         }
         else
         {
            bstt = FlyingAdcBms::SetBalancing(FlyingAdcBms::BAL_OFF);
            balanceCycles = 0;
         }
         Param::SetInt((Param::PARAM_NUM)(Param::u0cmd + chan), bstt);
      }
      else
      {
         FlyingAdcBms::SetBalancing(FlyingAdcBms::BAL_OFF);
      }
   }
   else
   {
      balanceCycles = totalBalanceCycles;
      bstt = FlyingAdcBms::SetBalancing(FlyingAdcBms::BAL_OFF);
      Param::SetInt((Param::PARAM_NUM)(Param::u0cmd + chan), bstt);
   }

   //Read cell voltage when balancing is turned off
   if (balanceCycles == totalBalanceCycles)
   {
      //Read ADC result before mux change
      float udc = FlyingAdcBms::GetRawResult();

      udc *= Param::GetFloat(Param::gain) / 1000.0f;

      Param::SetFloat((Param::PARAM_NUM)(Param::u0 + chan), udc);

      min = MIN(min, udc);
      max = MAX(max, udc);
      sum += udc;

      if ((chan + 1) >= Param::GetInt(Param::numchan))
      {
         chan = 0;
         avg = sum / Param::GetInt(Param::numchan);
         Accumulate(sum, min, max, avg);

         min = 8000;
         max = 0;
         sum = 0;
      }
      else
      {
         chan++;
      }

      FlyingAdcBms::SelectChannel(chan);

      FlyingAdcBms::StartAdc();
   }
}

//sample 100ms task
static void Ms100Task(void)
{
   static float estimatedSoc = 0;
   //The boot loader enables the watchdog, we have to reset it
   //at least every 2s or otherwise the controller is hard reset.
   iwdg_reset();
   float cpuLoad = scheduler->GetCpuLoad();
   Param::SetFloat(Param::cpuload, cpuLoad / 10);
   DigIo::led_out.Toggle();
   int sensor = Param::GetInt(Param::tempsns);

   if (sensor >= 0)
      Param::SetFloat(Param::temp, TempMeas::Lookup(AnaIn::temp2.Get(), (TempMeas::Sensors)sensor));
   else
      Param::SetInt(Param::temp, 255);

   BmsFsm::bmsstate stt = bmsFsm->Run((BmsFsm::bmsstate)Param::GetInt(Param::opmode));

   if (stt == BmsFsm::RUNBALANCE)
   {
      estimatedSoc = BmsAlgo::EstimateSocFromVoltage(Param::GetFloat(Param::umin));
      Param::SetFloat(Param::soc, estimatedSoc);
   }
   else
   {
      float asDiff = Param::GetFloat(Param::chargein) - Param::GetFloat(Param::chargeout);
      float soc = BmsAlgo::CalculateSocFromIntegration(estimatedSoc, asDiff);
      Param::SetFloat(Param::soc, soc);
   }

   if (bmsFsm->IsFirst())
   {
      CalculateCurrentLimits();
   }

   Param::SetInt(Param::opmode, stt);

   canMap->SendAll();
}

//sample 10 ms task
static void Ms25Task(void)
{
   if (Param::GetBool(Param::enable))
   {
      if (DigIo::muxena_out.Get())
         ReadAdc();
      else
         DigIo::muxena_out.Set();
   }
   else
   {
      FlyingAdcBms::MuxOff();
      DigIo::muxena_out.Clear();
   }
}

static void MeasureCurrent()
{
   int idcmode = Param::GetInt(Param::idcmode);
   //s32fp voltage = Param::Get(Param::udc);
   float current = 0;

   //if (rtc_get_counter_val() < 2) return; //Discard the first few current samples

   if (idcmode == IDC_DIFFERENTIAL || idcmode == IDC_SINGLE)
   {
      static int samples = 0;
      static u32fp amsIn = 0, amsOut = 0;
      static float idcavg = 0;
      int curpos = AnaIn::curpos.Get();
      int curneg = AnaIn::curneg.Get();
      float idcgain = Param::GetFloat(Param::idcgain);
      int idcofs = Param::GetInt(Param::idcofs);
      int rawCurrent = idcmode == IDC_SINGLE ? curpos : curpos - curneg;

      current = (rawCurrent - idcofs) / idcgain;

      if (current < -0.8f)
      {
         amsOut += -FP_FROMFLT(current);
      }
      else if (current > 0.8f)
      {
         amsIn += FP_FROMFLT(current);
      }

      idcavg += current;
      samples++;

      if (samples == 100)
      {
         s32fp chargein = Param::Get(Param::chargein);
         s32fp chargeout = Param::Get(Param::chargeout);

         chargein += amsIn / 100;
         chargeout += amsOut / 100;
         idcavg /= 100;

         //s32fp power = FP_MUL(voltage, idcavg);

         Param::SetFloat(Param::idcavg, idcavg);
         //Param::SetFlt(Param::power, power);

         amsIn = 0;
         amsOut = 0;
         samples = 0;
         idcavg = 0;

         //BmsCalculation::SetCharge(chargein, chargeout);
         Param::SetFixed(Param::chargein, chargein);
         Param::SetFixed(Param::chargeout, chargeout);
      }
   }
   else //Isa Shunt
   {
      //current = FP_FROMINT(IsaShunt::GetCurrent()) / 1000;
      //s32fp power = FP_MUL(voltage, current);
      //Param::SetFlt(Param::power, power);
   }

   Param::SetFloat(Param::idc, current);
}

/** This function is called when the user changes a parameter */
void Param::Change(Param::PARAM_NUM paramNum)
{
   switch (paramNum)
   {
   default:
      if (!currentSensorOn && Param::GetInt(Param::idcmode) > 0)
      {
         scheduler->AddTask(MeasureCurrent, 10);
         currentSensorOn = true;
      }

      BmsAlgo::SetNominalCapacity(Param::GetFloat(Param::nomcap));
      break;
   }
}

//Whichever timer(s) you use for the scheduler, you have to
//implement their ISRs here and call into the respective scheduler
extern "C" void tim2_isr(void)
{
   scheduler->Run();
}

extern "C" int main(void)
{
   clock_setup(); //Must always come first
   //rtc_setup();
   ANA_IN_CONFIGURE(ANA_IN_LIST);
   DIG_IO_CONFIGURE(DIG_IO_LIST);
   AnaIn::Start(); //Starts background ADC conversion via DMA
   write_bootloader_pininit(); //Instructs boot loader to initialize certain pins

   nvic_setup(); //Set up some interrupts
   spi_setup();
   parm_load(); //Load stored parameters

   Stm32Scheduler s(TIM2); //We never exit main so it's ok to put it on stack
   scheduler = &s;
   //Initialize CAN1, including interrupts. Clock must be enabled in clock_setup()
   Stm32Can c(CAN1, CanHardware::Baud500);
   CanMap cm(&c);
   canMap = &cm;

   BmsFsm fsm(&cm);
   c.AddReceiveCallback(&fsm);
   bmsFsm = &fsm;

   //This is all we need to do to set up a terminal on USART3
   TerminalCommands::SetCanMap(canMap);

   //Up to four tasks can be added to each timer scheduler
   //AddTask takes a function pointer and a calling interval in milliseconds.
   //The longest interval is 655ms due to hardware restrictions
   //You have to enable the interrupt (int this case for TIM2) in nvic_setup()
   //There you can also configure the priority of the scheduler over other interrupts
   s.AddTask(Ms25Task, 25);
   s.AddTask(Ms100Task, 100);

   //backward compatibility, version 4 was the first to support the "stream" command
   Param::SetInt(Param::version, 4);
   Param::Change(Param::PARAM_LAST); //Call callback one for general parameter propagation

   DigIo::selfena_out.Set();

   while(1)
   {
      char c = 0;

      if (canMap->GetPrintRequest() == PRINT_JSON)
      {
         TerminalCommands::PrintParamsJson(canMap, &c);
         canMap->SignalPrintComplete();
      }
   }

   return 0;
}

