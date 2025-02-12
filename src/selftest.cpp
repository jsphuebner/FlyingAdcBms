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
#include "selftest.h"
#include "flyingadcbms.h"
#include "my_math.h"

SelfTest::TestFunction SelfTest::testFunctions[] = {
   RunTestMuxOff, RunTestBalancer, TestCellConnection, TestCellConnection, NoTest
};

int SelfTest::cycleCounter = 0;
int SelfTest::numChannels = 16;
int SelfTest::errChannel = 0;
SelfTest::TestResult SelfTest::lastResult = SelfTest::TestOngoing;

/** \brief Runs a given self test
 *
 * \param testStep test to be run, increments to the next test when successful
 * \return TestResult
 *
 */
SelfTest::TestResult SelfTest::RunTest(int& testStep)
{
   lastResult = testFunctions[testStep]();

   if (lastResult == TestSuccess) {
      testStep++; //move to next test
      cycleCounter = 0;
   }
   else if (lastResult == TestOngoing) {
      cycleCounter++;
   }
   else {
      //last test or failed
      //nothing to do, must be handled upstream
   }

   return lastResult;
}

/** \brief Turn off mux and read ADC result. It must be close to 0
 *
 * \return TestResult
 *
 */
SelfTest::TestResult SelfTest::RunTestMuxOff()
{
   if (cycleCounter == 0) {
      FlyingAdcBms::MuxOff();
      FlyingAdcBms::SetBalancing(FlyingAdcBms::BAL_DISCHARGE);
      FlyingAdcBms::StartAdc();
   }
   else if (cycleCounter == 1) {
      int adc = FlyingAdcBms::GetResult();
      adc = ABS(adc);

      if (adc < 5) //We expect no voltage on the ADC
         return TestSuccess;
      else
         return TestFailed;
   }
   return TestOngoing;
}

/** \brief Test if balancer circuit works
 *
 * \return TestResult
 *
 */
SelfTest::TestResult SelfTest::RunTestBalancer()
{
   if (cycleCounter == 0) {
      FlyingAdcBms::MuxOff();
      FlyingAdcBms::SetBalancing(FlyingAdcBms::BAL_CHARGE);
      FlyingAdcBms::StartAdc();
   }
   else if (cycleCounter == 2) {
      int adc = FlyingAdcBms::GetResult();

      if (adc < 8190) //We expect the ADC to saturate
         return TestFailed;
   }
   else if (cycleCounter == 3) {
      FlyingAdcBms::SelectChannel(1); //this leads to negative voltage
      FlyingAdcBms::MuxOff(); //but we turn off the mux right away
      FlyingAdcBms::SetBalancing(FlyingAdcBms::BAL_CHARGE);
      FlyingAdcBms::StartAdc();
   }
   else if (cycleCounter == 5) {
      int adc = FlyingAdcBms::GetResult();
      FlyingAdcBms::SetBalancing(FlyingAdcBms::BAL_OFF);

      if (adc < 8190) //We expect the ADC to saturate
         return TestFailed;
      else
         return TestSuccess;
   }
   return TestOngoing;
}

SelfTest::TestResult SelfTest::TestCellConnection()
{
   static bool overVoltage = false;
   static bool polarityCheckComplete = false;
   int channel = cycleCounter / 2;

   if (overVoltage) return TestFailed; //make this look like a separate test
   if (polarityCheckComplete) return TestSuccess;

   if (cycleCounter & 1) //on odd cycles measure, on even cycles switch mux
   {
      int adc = FlyingAdcBms::GetResult();
      FlyingAdcBms::MuxOff();

      if (adc < -1000) {
         errChannel = channel;
         return TestFailed;
      }
      if (adc > 7500) {
         overVoltage = true;
         errChannel = channel;
         return TestSuccess; //report polarity check as good, but over voltage check as failed on the next call
      }
      if (channel == (numChannels - 1)) {
         polarityCheckComplete = true;
         return TestSuccess;
      }
   }
   else
   {
      FlyingAdcBms::SelectChannel(channel);
      FlyingAdcBms::StartAdc();
   }
   return TestOngoing;
}

/** \brief Last test, always return done
 *
 * \return TestResult
 *
 */
SelfTest::TestResult SelfTest::NoTest()
{
   return TestsDone;
}
