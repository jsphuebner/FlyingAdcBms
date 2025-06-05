/*
 * This file is part of the FlyingAdcBms project.
 *
 * Copyright (C) 2025 Johannes Huebner <dev@johanneshuebner.com>
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
#include <libopencm3/stm32/rtc.h>
#include <libopencm3/stm32/can.h>
#include <libopencm3/stm32/iwdg.h>
#include <libopencm3/stm32/f1/bkp.h>
#include "stm32_can.h"
#include "canmap.h"
#include "cansdo.h"
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
#include "sdocommands.h"
#include "flyingadcbms.h"
#include "bmsfsm.h"
#include "bmsalgo.h"
#include "bmsio.h"
#include "selftest.h"

#define PRINT_JSON 0

static Stm32Scheduler* scheduler;
static CanMap* canMapExternal;
static CanMap* canMapInternal;
static BmsFsm* bmsFsm;
HwRev hwRev;

static void CalculateCurrentLimits()
{
   float chargeCurrentLimit = BmsAlgo::GetChargeCurrent(Param::GetFloat(Param::umax));
   chargeCurrentLimit *= BmsAlgo::LowTemperatureDerating(Param::GetFloat(Param::tempmin));
   chargeCurrentLimit *= BmsAlgo::HighTemperatureDerating(Param::GetFloat(Param::tempmax), 50);
   Param::SetFloat(Param::chargelim, chargeCurrentLimit);

   float dischargeCurrentLimit = Param::GetFloat(Param::dischargemax);
   dischargeCurrentLimit *= BmsAlgo::LimitMinimumCellVoltage(Param::GetFloat(Param::umin), Param::GetFloat(Param::ucellmin));
   dischargeCurrentLimit *= BmsAlgo::HighTemperatureDerating(Param::GetFloat(Param::tempmax), 53);
   Param::SetFloat(Param::dischargelim, dischargeCurrentLimit);
/*
   if (Param::GetFloat(Param::umax) < (Param::GetFloat(Param::ucellmax) - 50))
      DigIo::nextena_out.Set();
   else if (Param::GetFloat(Param::umax) >= Param::GetFloat(Param::ucellmax))
      DigIo::nextena_out.Clear();*/
}

static void CalculateSocSoh(BmsFsm::bmsstate stt, BmsFsm::bmsstate laststt)
{
   static float estimatedSoc = 0, estimatedSocAtValidSoh = -1, asDiffAfterEstimate = 0, soh = 0;
   float asDiff = Param::GetFloat(Param::chargein) - Param::GetFloat(Param::chargeout);

   if (estimatedSoc == 0)
   {
      estimatedSoc = Param::GetFloat(Param::soc);
      estimatedSocAtValidSoh = estimatedSoc;
   }

   /* if we change over from IDLE to RUN we have to stop all estimation processes
      because there is now current through the battery again, skewing open voltage readings.
      So the estimations do not get any better at this point and we store the results */
   if (laststt == BmsFsm::IDLE && stt == BmsFsm::RUN)
   {
      /* Remember the Ampere Seconds at the point of the last estimation
         in order to be prepared for the next estimation */
      asDiffAfterEstimate = asDiff;

      /* If the SoC difference was large enough we have a valid SoH */
      if (soh > 0)
      {
         float lastSoh = (float)BKP_DR2 / 100.0f;
         //Don't just overwrite the existing SoH but average it to the existing SoH with a slow IIR filter
         soh = IIRFILTERF(lastSoh, soh, 10);
         //Store in NVRAM
         BKP_DR2 = (uint16_t)(soh * 100);
         Param::SetFloat(Param::soh, soh);
         Param::SetFloat(Param::sohpreset, soh);
         /* Remember SoC at the point of this estimation
            in order to be prepared for the next estimation */
         estimatedSocAtValidSoh = estimatedSoc;
         //BmsAlgo::SetNominalCapacity(Param::GetFloat(Param::nomcap) * soh / 100.0f);
      }
   }

   /* IDLE state means we haven't seen any current for some (configurable) time
      so cell voltage is approaching the true open circuit voltage */
   if (stt == BmsFsm::IDLE && Param::GetFloat(Param::idc) < 0.8f)
   {
      estimatedSoc = BmsAlgo::EstimateSocFromVoltage(Param::GetFloat(Param::umin));
      Param::SetFloat(Param::soc, estimatedSoc);
      //Store estimated SoC in NVRAM
      BKP_DR1 = (uint16_t)(estimatedSoc * 100);

      soh = BmsAlgo::CalculateSoH(estimatedSocAtValidSoh, estimatedSoc, asDiff - asDiffAfterEstimate);

      if (estimatedSocAtValidSoh < 0)
         estimatedSocAtValidSoh = estimatedSoc;
   }
   else
   {
      float soc = BmsAlgo::CalculateSocFromIntegration(estimatedSoc, asDiff - asDiffAfterEstimate);
      Param::SetFloat(Param::soc, soc);
      BKP_DR1 = (uint16_t)(soc * 100);
   }
}

static void Ms100Task(void)
{
   static uint8_t ledDivider = 0;
   //The boot loader enables the watchdog, we have to reset it
   //at least every 2s or otherwise the controller is hard reset.
   iwdg_reset();
   float cpuLoad = scheduler->GetCpuLoad();
   Param::SetFloat(Param::cpuload, cpuLoad / 10);

   if (Param::GetInt(Param::opmode) != BmsFsm::ERROR)
      DigIo::led_out.Toggle();
   else //blink slower when an error is detected
   {
      if (ledDivider == 0)
      {
         DigIo::led_out.Toggle();
         ledDivider = 4;
      }
      else
         ledDivider--;
   }

   BmsFsm::bmsstate laststt = (BmsFsm::bmsstate)Param::GetInt(Param::opmode);
   BmsFsm::bmsstate stt = bmsFsm->Run(laststt);
   BmsIO::ReadTemperatures();

   if (bmsFsm->IsFirst())
   {
      CalculateCurrentLimits();
      CalculateSocSoh(stt, laststt);
   }

   Param::SetInt(Param::opmode, stt);
   //4 bit circular counter for alive indication
   Param::SetInt(Param::counter, (Param::GetInt(Param::counter) + 1) & 0xF);
   Param::SetInt(Param::uptime, rtc_get_counter_val());

   canMapExternal->SendAll();
   canMapInternal->SendAll();
}

static void RunSelfTest()
{
   static int test = 0;
   if (SelfTest::GetLastResult() == SelfTest::TestFailed) return; //do not call anymore tests
   SelfTest::TestResult result = SelfTest::RunTest(test);

   if (result == SelfTest::TestFailed)
   {
      ErrorMessage::Post((ERROR_MESSAGE_NUM)(test + 1));
      Param::SetInt(Param::lasterr, test + 1);
      Param::SetInt(Param::errinfo, SelfTest::GetErrorChannel());
   }
}

/** \brief This task runs the BMS voltage sensing */
static void ReadCellVoltages(void)
{
   int opmode = Param::GetInt(Param::opmode);
   int testchan = Param::GetInt(Param::testchan);

   if (opmode == BmsFsm::SELFTEST)
      RunSelfTest();
   else if (testchan >= 0)
      BmsIO::TestReadCellVoltage(testchan, (FlyingAdcBms::BalanceCommand)Param::GetInt(Param::testbalance));
   else if (Param::GetBool(Param::enable) && (opmode == BmsFsm::RUN || opmode == BmsFsm::IDLE))
      BmsIO::ReadCellVoltages();
   else
      FlyingAdcBms::MuxOff();
}

/** This function is called when the user changes a parameter */
void Param::Change(Param::PARAM_NUM paramNum)
{
   switch (paramNum)
   {
   case Param::sohpreset:
      Param::SetFloat(Param::soh, Param::GetFloat(Param::sohpreset));
      break;
   default:
      BmsAlgo::SetNominalCapacity(Param::GetFloat(Param::nomcap) * Param::GetFloat(Param::soh) / 100.0f);
      SelfTest::SetNumChannels(Param::GetInt(Param::numchan));

      for (int i = 0; i < 11; i++)
      {
         BmsAlgo::SetSocLookupPoint(i * 10, Param::GetInt((Param::PARAM_NUM)(Param::ucell0soc + i)));
      }

      BmsAlgo::SetCCCVCurve(0, Param::GetFloat(Param::icc1), Param::GetInt(Param::ucv1));
      BmsAlgo::SetCCCVCurve(1, Param::GetFloat(Param::icc2), Param::GetInt(Param::ucv2));
      BmsAlgo::SetCCCVCurve(2, Param::GetFloat(Param::icc3), Param::GetInt(Param::ucellmax));
      break;
   }
}

static void LoadNVRAM()
{
   float soc = (float)BKP_DR1 / 100.0f;

   if (soc >= 0 && soc <= 100)
      Param::SetFloat(Param::soc, soc);

   if (BKP_DR2 != 0)
      Param::SetFloat(Param::soh, (float)BKP_DR2 / 100.0f);
   else
      Param::SetFixed(Param::soh, Param::Get(Param::sohpreset));
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
   rtc_setup();
   hwRev = detect_hw();
   ANA_IN_CONFIGURE(ANA_IN_LIST);
   DIG_IO_CONFIGURE(DIG_IO_LIST);
   #ifdef HWV1
   spi_setup(); //in case we use V1 hardware
   DigIo::led_out.Configure(GPIOB, GPIO1, PinMode::OUTPUT);
   #endif // HWV1
   DigIo::selfena_out.Set();
   AnaIn::Start(); //Starts background ADC conversion via DMA
   write_bootloader_pininit(); //Instructs boot loader to initialize certain pins
   //JTAG must be turned off as it steals PB4
   gpio_primary_remap(AFIO_MAPR_SWJ_CFG_JTAG_OFF_SW_ON, 0);

   nvic_setup(); //Set up some interrupts
   parm_load(); //Load stored parameters

   Stm32Scheduler s(TIM2); //We never exit main so it's ok to put it on stack
   scheduler = &s;
   //Initialize CAN1, including interrupts. Clock must be enabled in clock_setup()
   Stm32Can c(CAN1, CanHardware::Baud500);
   CanMap cmi(&c, false);
   CanMap cme(&c);
   canMapInternal = &cmi;
   canMapExternal = &cme;
   CanSdo sdo(&c, &cme);

   BmsFsm fsm(&cmi, &sdo);
   //c.AddCallback(&fsm);
   bmsFsm = &fsm;
   BmsIO::SetBmsFsm(&fsm);

   TerminalCommands::SetCanMap(canMapExternal);
   SdoCommands::SetCanMap(canMapExternal);

   s.AddTask(BmsIO::MeasureCurrent, 5);
   s.AddTask(ReadCellVoltages, 25);
   s.AddTask(BmsIO::SwitchMux, 2); //This must added after ReadCellVoltages() to avoid an additional 2 ms delay
   s.AddTask(Ms100Task, 100);

   Param::SetInt(Param::hwrev, hwRev);
   Param::SetInt(Param::version, 4);
   Param::Change(Param::PARAM_LAST); //Call callback once for general parameter propagation

   LoadNVRAM();

   while(1)
   {
      char c = 0;
      CanSdo::SdoFrame* sdoFrame = sdo.GetPendingUserspaceSdo();

      if (sdo.GetPrintRequest() == PRINT_JSON)
      {
         TerminalCommands::PrintParamsJson(&sdo, &c);
      }
      if (0 != sdoFrame)
      {
         SdoCommands::ProcessStandardCommands(sdoFrame);
         sdo.SendSdoReply(sdoFrame);
      }
   }

   return 0;
}

/* to solve linker warning, see https://openinverter.org/forum/viewtopic.php?p=64546#p64546 */
extern "C" void __cxa_pure_virtual() { while (1); }
