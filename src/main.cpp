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

#define PRINT_JSON 0

static Stm32Scheduler* scheduler;
static CanMap* canMap;

static void ReadAdc()
{
   int totalBalanceCycles = 10;
   static uint8_t chan = 0, balanceCycles = 0;
   static float sum = 0, min, max, avg;
   bool balance = Param::GetBool(Param::balance);
   FlyingAdcBms::BalanceStatus bstt = FlyingAdcBms::STT_OFF;

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

         //Keep clocking the mux so it doesn't turn off
         //spi_xfer(SPI1, 0x00C0 | chan);

         if (udc < (avg - 1))
         {
            bstt = FlyingAdcBms::SetBalancing(FlyingAdcBms::BAL_CHARGE);
         }
         else if (udc > (avg + 1))
         {
            bstt = FlyingAdcBms::SetBalancing(FlyingAdcBms::BAL_DISCHARGE);
         }
         else
         {
            FlyingAdcBms::SetBalancing(FlyingAdcBms::BAL_OFF);
         }
      }
      else
      {
         FlyingAdcBms::SetBalancing(FlyingAdcBms::BAL_OFF);
      }
   }
   else
   {
      balanceCycles = totalBalanceCycles;
      FlyingAdcBms::SetBalancing(FlyingAdcBms::BAL_OFF);
   }

   Param::SetInt((Param::PARAM_NUM)(Param::u0cmd + chan), bstt);

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
         Param::SetFloat(Param::uavg, avg);
         Param::SetFloat(Param::umin, min);
         Param::SetFloat(Param::umax, max);
         Param::SetFloat(Param::udelta, max - min);

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
   //The boot loader enables the watchdog, we have to reset it
   //at least every 2s or otherwise the controller is hard reset.
   iwdg_reset();
   float cpuLoad = scheduler->GetCpuLoad();
   Param::SetFloat(Param::cpuload, cpuLoad / 10);

   canMap->SendAll();
}

//sample 10 ms task
static void Ms25Task(void)
{
   if (Param::GetBool(Param::enable))
      ReadAdc();
   else
      FlyingAdcBms::MuxOff();
}

/** This function is called when the user changes a parameter */
void Param::Change(Param::PARAM_NUM paramNum)
{
   switch (paramNum)
   {
   default:
      //Handle general parameter changes here. Add paramNum labels for handling specific parameters
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

   cm.SetNodeId(3);

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

   //Now all our main() does is running the terminal
   //All other processing takes place in the scheduler or other interrupt service routines
   //The terminal has lowest priority, so even loading it down heavily will not disturb
   //our more important processing routines.
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

