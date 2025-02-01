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
#include "flyingadcbms.h"
#include "bmsfsm.h"
#include "bmsalgo.h"
#include "temp_meas.h"
#include "selftest.h"

#define PRINT_JSON 0
#define NO_TEMP    128

static Stm32Scheduler* scheduler;
static CanMap* canMapExternal;
static CanMap* canMapInternal;
static BmsFsm* bmsFsm;

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
         //Here we undo the local average calculation on the module to calculate the substrings total voltage
         totalSum += Param::GetFloat(bmsFsm->GetDataItem(Param::uavg0, i)) * bmsFsm->GetCellsOfModule(i);
         totalMin = MIN(totalMin, Param::GetFloat(bmsFsm->GetDataItem(Param::umin0, i)));
         totalMax = MAX(totalMax, Param::GetFloat(bmsFsm->GetDataItem(Param::umax0, i)));
      }

      float tempmin = NO_TEMP, tempmax = -40;

      for (int i = 0; i < bmsFsm->GetNumberOfModules(); i++)
      {
         float tempmin0 = Param::GetFloat(bmsFsm->GetDataItem(Param::tempmin0, i));
         float tempmax0 = Param::GetFloat(bmsFsm->GetDataItem(Param::tempmax0, i));

         if (tempmin0 < NO_TEMP)
         {
            tempmin = MIN(tempmin, tempmin0);
            tempmax = MAX(tempmax, tempmax0);
         }
      }

      Param::SetFloat(Param::umin, totalMin);
      Param::SetFloat(Param::umax, totalMax);
      Param::SetFloat(Param::uavg, totalSum / Param::GetInt(Param::totalcells));
      Param::SetFloat(Param::udelta, totalMax - totalMin);
      Param::SetFloat(Param::utotal, totalSum);
      Param::SetFloat(Param::tempmin, tempmin);
      Param::SetFloat(Param::tempmax, tempmax);
   }
   else //if we are a sub module write averages straight to data module
   {
      Param::SetFloat(Param::utotal, sum);
      Param::SetFloat(Param::uavg0, avg);
      Param::SetFloat(Param::umin0, min);
      Param::SetFloat(Param::umax0, max);
      Param::SetFloat(Param::udelta, max - min);
   }
}

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
}

static void ReadAdc()
{
   int totalBalanceCycles = 30;
   static uint8_t chan = 0, balanceCycles = 0;
   static float sum = 0, min, max, avg;
   int balMode = Param::GetInt(Param::balmode);
   bool balance = Param::GetInt(Param::opmode) == BmsFsm::IDLE && Param::GetFloat(Param::uavg) > Param::GetFloat(Param::ubalance) && BAL_OFF != balMode;
   FlyingAdcBms::BalanceStatus bstt;

   if (balance)
   {
      if (balanceCycles == 0)
      {
         balanceCycles = totalBalanceCycles; //this leads to switching to next channel below
      }
      else
      {
         balanceCycles--;
      }

      if (balanceCycles > 0 && balanceCycles < (totalBalanceCycles - 1))
      {
         float udc = Param::GetFloat((Param::PARAM_NUM)(Param::u0 + chan));
         float balanceTarget = 0;

         switch (balMode)
         {
         case BAL_ADD: //maximum cell voltage is target when only adding
            balanceTarget = Param::GetFloat(Param::umax);
            break;
         case BAL_DIS: //minimum cell voltage is target when only dissipating
            balanceTarget = Param::GetFloat(Param::umin);
            break;
         case BAL_BOTH: //average cell voltage is target when dissipating and adding
            balanceTarget = Param::GetFloat(Param::uavg);
            break;
         default: //not balancing
            break;
         }

         if (udc < (balanceTarget - 3) && (balMode & BAL_ADD))
         {
            bstt = FlyingAdcBms::SetBalancing(FlyingAdcBms::BAL_CHARGE);
         }
         else if (udc > (balanceTarget + 1) && (balMode & BAL_DIS))
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
      float gain = Param::GetFloat(Param::gain);

      if (chan == 0)
         gain *= 1 + Param::GetFloat(Param::correction0) / 1000000.0f;
      else if (chan == 1)
         gain *= 1 + Param::GetFloat(Param::correction1) / 1000000.0f;
      else if (chan == 15)
         gain *= 1 + Param::GetFloat(Param::correction15) / 1000000.0f;

      //Read ADC result before mux change
      float udc = FlyingAdcBms::GetResult() * (gain / 1000.0f);

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

static void TestAdc(int chan)
{
   float gain = Param::GetFloat(Param::gain);

   if (chan == 0)
      gain *= 1 + Param::GetFloat(Param::correction0) / 1000000.0f;
   else if (chan == 1)
      gain *= 1 + Param::GetFloat(Param::correction1) / 1000000.0f;
   else if (chan == 15)
      gain *= 1 + Param::GetFloat(Param::correction15) / 1000000.0f;

   float udc = FlyingAdcBms::GetResult() * (gain / 1000.0f);;
   FlyingAdcBms::SelectChannel(chan);
   FlyingAdcBms::StartAdc();
   Param::SetFloat((Param::PARAM_NUM)(Param::u0 + chan), udc);
}

static void CalculateSocSoh(BmsFsm::bmsstate stt)
{
   static float estimatedSoc = 0, estimatedSocAtValidSoh = -1;
   float asDiff = Param::GetFloat(Param::chargein) - Param::GetFloat(Param::chargeout);

   if (stt == BmsFsm::IDLE && Param::GetFloat(Param::idc) < 0.8f)
   {
      float lastSoh = Param::GetFloat(Param::soh);
      estimatedSoc = BmsAlgo::EstimateSocFromVoltage(Param::GetFloat(Param::umin));
      Param::SetFloat(Param::soc, estimatedSoc);

      float soh = BmsAlgo::CalculateSoH(estimatedSocAtValidSoh, estimatedSoc, asDiff);

      if (estimatedSocAtValidSoh < 0)
         estimatedSocAtValidSoh = estimatedSoc;

      if (soh > 0)
      {
         soh = IIRFILTERF(lastSoh, soh, 10);
         BKP_DR2 = (uint16_t)(soh * 100);
         Param::SetFloat(Param::soh, soh);
         estimatedSocAtValidSoh = estimatedSoc;
         Param::SetInt(Param::chargein, 0);
         Param::SetInt(Param::chargeout, 0);
      }
   }
   else
   {
      float soc = BmsAlgo::CalculateSocFromIntegration(estimatedSoc, asDiff);
      Param::SetFloat(Param::soc, soc);
   }
}

static void ReadTemperatures()
{
   int sensor = Param::GetInt(Param::tempsns);
   int nomRes = Param::GetInt(Param::tempres);
   int beta = Param::GetInt(Param::tempbeta);
   float temp1 = NO_TEMP, temp2 = NO_TEMP, tempmin = NO_TEMP, tempmax = NO_TEMP;

   if (sensor & 1)
      tempmin = tempmax = temp1 = TempMeas::AdcToTemperature(AnaIn::temp1.Get(), nomRes, beta);

   if (sensor & 2)
      tempmin = tempmax = temp2 = TempMeas::AdcToTemperature(AnaIn::temp2.Get(), nomRes, beta);

   if (sensor == 3) //two sensors, calculate min and max
   {
      tempmin = MIN(temp1, temp2);
      tempmax = MAX(temp1, temp2);
   }

   Param::SetFloat(Param::tempmin0, tempmin);
   Param::SetFloat(Param::tempmax0, tempmax);
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

   BmsFsm::bmsstate stt = bmsFsm->Run((BmsFsm::bmsstate)Param::GetInt(Param::opmode));
   ReadTemperatures();

   if (bmsFsm->IsFirst())
   {
      CalculateCurrentLimits();
      CalculateSocSoh(stt);
   }

   Param::SetInt(Param::opmode, stt);
   //4 bit circular counter for alive indication
   Param::SetInt(Param::counter, (Param::GetInt(Param::counter) + 1) & 0xF);

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
      Param::SetInt(Param::errchan, SelfTest::GetErrorChannel());
   }
}

/** \brief This task runs the BMS voltage sensing */
static void Ms25Task(void)
{
   int opmode = Param::GetInt(Param::opmode);
   int testchan = Param::GetInt(Param::testchan);

   if (opmode == BmsFsm::SELFTEST)
      RunSelfTest();
   else if (testchan >= 0)
      TestAdc(testchan);
   else if (Param::GetBool(Param::enable) && (opmode == BmsFsm::RUN || opmode == BmsFsm::IDLE))
      ReadAdc();
   else
      FlyingAdcBms::MuxOff();
}

static void MeasureCurrent()
{
   int idcmode = Param::GetInt(Param::idcmode);

   if (idcmode == IDC_DIFFERENTIAL || idcmode == IDC_SINGLE)
   {
      float current = 0;
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

      if (samples == 200)
      {
         s32fp chargein = Param::Get(Param::chargein);
         s32fp chargeout = Param::Get(Param::chargeout);

         chargein += amsIn / 200;
         chargeout += amsOut / 200;
         idcavg /= 200;

         float voltage = Param::GetFloat(Param::utotal) / 1000;
         float power = voltage * idcavg;

         Param::SetFloat(Param::idcavg, idcavg);
         Param::SetFloat(Param::power, power);

         amsIn = 0;
         amsOut = 0;
         samples = 0;
         idcavg = 0;

         //BmsCalculation::SetCharge(chargein, chargeout);
         Param::SetFixed(Param::chargein, chargein);
         Param::SetFixed(Param::chargeout, chargeout);
      }
      Param::SetFloat(Param::idc, current);
   }
}

/** This function is called when the user changes a parameter */
void Param::Change(Param::PARAM_NUM paramNum)
{
   switch (paramNum)
   {
   default:
      BmsAlgo::SetNominalCapacity(Param::GetFloat(Param::nomcap));
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

   TerminalCommands::SetCanMap(canMapExternal);

   s.AddTask(MeasureCurrent, 5);
   s.AddTask(Ms25Task, 25);
   s.AddTask(Ms100Task, 100);

   Param::SetInt(Param::version, 4);
   Param::Change(Param::PARAM_LAST); //Call callback once for general parameter propagation

   if (BKP_DR2 != 0)
      Param::SetFloat(Param::soh, (float)BKP_DR2 / 100.0f);
   else
      Param::SetInt(Param::soh, 100);

   while(1)
   {
      char c = 0;

      if (sdo.GetPrintRequest() == PRINT_JSON)
      {
         TerminalCommands::PrintParamsJson(&sdo, &c);
      }
   }

   return 0;
}

/* to solve linker warning, see https://openinverter.org/forum/viewtopic.php?p=64546#p64546 */
extern "C" void __cxa_pure_virtual() { while (1); }
