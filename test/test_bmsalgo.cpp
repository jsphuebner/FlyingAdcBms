/*
 * This file is part of the tumanako_vc project.
 *
 * Copyright (C) 2010 Johannes Huebner <contact@johanneshuebner.com>
 * Copyright (C) 2010 Edward Cheeseman <cheesemanedward@gmail.com>
 * Copyright (C) 2009 Uwe Hermann <uwe@hermann-uwe.de>
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

#include "test.h"
#include "bmsalgo.h"

class BmsAlgoTest: public UnitTest
{
   public:
      BmsAlgoTest(const std::list<VoidFunction>* cases): UnitTest(cases) {}
      virtual void TestCaseSetup();
};

void BmsAlgoTest::TestCaseSetup()
{
   uint16_t socLookup[] = { 3300, 3400, 3450, 3500, 3560, 3600, 3700, 3800, 4000, 4100, 4200 };

   for (int i = 0; i < 10; i++)
      BmsAlgo::SetSocLookupPoint(i * 10, socLookup[i]);

   BmsAlgo::SetNominalCapacity(100);
   BmsAlgo::SetMinVoltage(3300, 100);
   BmsAlgo::SetControllerGains(1, 1);
   BmsAlgo::SetCCCVCurve(0, 400, 3900);
   BmsAlgo::SetCCCVCurve(1, 200, 4100);
   BmsAlgo::SetCCCVCurve(2, 100, 4200);
}

static void TestEstimateSocFromVoltage()
{
   float soc = BmsAlgo::EstimateSocFromVoltage(3650);
   ASSERT(soc == 55);
   soc = BmsAlgo::EstimateSocFromVoltage(3000);
   ASSERT(soc == 0);
   soc = BmsAlgo::EstimateSocFromVoltage(4300);
   ASSERT(soc == 100);
}

static void TestCalculateSocFromIntegration()
{
   float soc = BmsAlgo::CalculateSocFromIntegration(50, 1.5 * 3600); //Add 1.5 Ah to 100 Ah battery
   ASSERT(soc == 51.5);
   soc = BmsAlgo::CalculateSocFromIntegration(50, -10 * 3600); //Take 10 Ah to 100 Ah battery
   ASSERT(soc == 40);
   soc = BmsAlgo::CalculateSocFromIntegration(10, -20 * 3600); //Take 20 Ah to 100 Ah battery that is charged to 10%
   ASSERT(soc == 0); //test saturation
   soc = BmsAlgo::CalculateSocFromIntegration(90, 20 * 3600); //Add 20 Ah to 100 Ah battery that is charged to 90%
   ASSERT(soc == 102); //test saturation allowing slight overrun
}

static void TestCalculateSoH()
{
   float soh = BmsAlgo::CalculateSoH(10, 20, 3600); //too little SoC diff for calculation
   ASSERT(soh < 0);
   soh = BmsAlgo::CalculateSoH(40, 70, 30 * 3600); //simulate 100% SoH (SoC diff matches Ah diff)
   ASSERT(soh == 100);
   soh = BmsAlgo::CalculateSoH(40, 70, 0.9 * 30 * 3600); //simulate 90% SoH (SoC diff greater than Ah diff)
   ASSERT(soh == 90);
   soh = BmsAlgo::CalculateSoH(40, 70, 1.1 * 30 * 3600); //simulate 110% SoH (SoC diff smaller than Ah diff)
   ASSERT(soh == 110);
}

static void TestGetChargeCurrent1()
{
   float current;
   for (int i = 0; i < 30; i++) //run 20 loops for integrator to reach steady state
      current = BmsAlgo::GetChargeCurrent(3800, 3800, 0); //100 mV away from CV point
   ASSERT(current == 400);
}

static void TestGetChargeCurrent2()
{
   float current;

   for (int i = 0; i < 500; i++) //run 200 loops (20s) for integrator to reach steady state
      current = BmsAlgo::GetChargeCurrent(3850 + current * 0.15, 4200, 0); //simulate internal resistance
   ASSERT(current == 333); //333A because 3850 + 333 * 0.15 == 3900

   for (int i = 0; i < 30; i++) //run 10 loops (1s) for integrator to reach steady state
      current = BmsAlgo::GetChargeCurrent(3900 + current * 0.15, 4200, 0); //simulate internal resistance
   ASSERT(current == 200); //in second CCCV stage

   for (int i = 0; i < 150; i++) //run 50 loops (5s) for integrator to reach steady state
      current = BmsAlgo::GetChargeCurrent(4205 + current * 0.15, 4200, 0); //simulate internal resistance
   ASSERT(current == 0); //333A because 3850 + 333 * 0.15 == 3900
}

static void TestLimitMinimumCellVoltage()
{
   float current;
   for (int i = 0; i < 50; i++) //run 50 loops (5s) for integrator to reach steady state
      current = BmsAlgo::LimitMinimumCellVoltage(3300);
   ASSERT(current == 0);

   for (int i = 0; i < 50; i++) //run 50 loops (5s) for integrator to reach steady state
      current = BmsAlgo::LimitMinimumCellVoltage(3200);
   ASSERT(current == 0);

   for (int i = 0; i < 500; i++) //run 50 loops (5s) for integrator to reach steady state
      current = BmsAlgo::LimitMinimumCellVoltage(3310 - current * 0.15); //simulate internal resistance
   ASSERT(current == 67); //67A because 3310 + 67 * 0.15 == 3300

   for (int i = 0; i < 50; i++) //run 50 loops (5s) for integrator to reach steady state
      current = BmsAlgo::LimitMinimumCellVoltage(4200);
   ASSERT(current == 100);
}

static void TestLowTemperatureDerating()
{
   float factor = BmsAlgo::LowTemperatureDerating(-20);
   ASSERT(factor == 0);
   factor = BmsAlgo::LowTemperatureDerating(-100);
   ASSERT(factor == 0);
   factor = BmsAlgo::LowTemperatureDerating(-10);
   ASSERT(ABS(factor - 0.15) < 0.01); //Account for rounding errors
   factor = BmsAlgo::LowTemperatureDerating(0);
   ASSERT(ABS(factor - 0.3) < 0.01); //Account for rounding errors
   factor = BmsAlgo::LowTemperatureDerating(10);
   ASSERT(ABS(factor - 0.58) < 0.01); //Account for rounding errors
   factor = BmsAlgo::LowTemperatureDerating(25);
   ASSERT(factor == 1); //Account for rounding errors
   factor = BmsAlgo::LowTemperatureDerating(100);
   ASSERT(factor == 1); //Account for rounding errors
}

static void TestHighTemperatureDerating()
{
   float factor = BmsAlgo::HighTemperatureDerating(0, 50);
   ASSERT(factor == 1);
   factor = BmsAlgo::HighTemperatureDerating(43.3, 50);
   ASSERT(factor == 1);
   factor = BmsAlgo::HighTemperatureDerating(46.6667, 50);
   ASSERT(ABS(factor - 0.5) < 0.01); //Account for rounding errors
   factor = BmsAlgo::HighTemperatureDerating(50, 50);
   ASSERT(factor == 0);
   factor = BmsAlgo::HighTemperatureDerating(80, 50);
   ASSERT(factor == 0);
}

//This line registers the test
REGISTER_TEST(BmsAlgoTest, TestEstimateSocFromVoltage, TestCalculateSocFromIntegration,
              TestCalculateSoH, TestGetChargeCurrent1, TestGetChargeCurrent2,
              TestLimitMinimumCellVoltage, TestLowTemperatureDerating, TestHighTemperatureDerating);
