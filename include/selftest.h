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
#ifndef SELFTEST_H
#define SELFTEST_H


class SelfTest
{
   public:
      enum TestResult { TestOngoing, TestSuccess, TestFailed, TestsDone };
      static TestResult RunTest(int& testStep);
      static TestResult GetLastResult() { return lastResult; }

   private:
      typedef TestResult (*TestFunction)(void);

      static TestResult RunTestMuxOff();
      static TestResult RunTestBalancer();
      static TestResult NoTest();
      static TestResult TestCellConnection();

      static TestFunction testFunctions[];
      static int cycleCounter;
      static TestResult lastResult;
};

#endif // SELFTEST_H
