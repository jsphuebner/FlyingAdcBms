/*
 * This file is part of the stm32-sine project.
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
#define __TEMP_LU_TABLES
#include "temp_meas.h"
#include <stdint.h>

#define TABLEN(a) sizeof(a) / sizeof(a[0])
#define FIRST_MOTOR_SENSOR TEMP_KTY83

enum coeff { PTC, NTC };

typedef struct TempSensor
{
   int16_t tempMin;
   int16_t tempMax;
   uint8_t step;
   uint8_t tabSize;
   enum coeff coeff;
   const uint16_t *lookup;
} TEMP_SENSOR;

/* Temp sensor with JCurve */
static const uint16_t JCurve[] = { JCURVE };

/* contributed by Fabian Brauss */
/* Temp sensor KTY81-121 */
static const uint16_t Kty81[] = { KTY81 };

/* Temp sensor PT1000 */
static const uint16_t Pt1000[] = { PT1000 };

/* Temp sensor used on leaf batteries */
static const uint16_t leaf[] = { LEAF_BATT };

static const TEMP_SENSOR sensors[] =
{
   { -25, 105, 5,  TABLEN(JCurve),         NTC, JCurve     },
   { -50, 150, 10, TABLEN(Kty81),          NTC, Kty81      },
   { -50, 150, 10, TABLEN(Pt1000),         NTC, Pt1000     },
   { -20, 60,  5,  TABLEN(leaf),           NTC, leaf       }
};

float TempMeas::Lookup(int digit, Sensors sensorId)
{
   if (sensorId >= TEMP_LAST) return 0;
   int index = sensorId;

   const TEMP_SENSOR * sensor = &sensors[index];
   uint16_t last;

   for (uint32_t i = 0; i < sensor->tabSize; i++)
   {
      uint16_t cur = sensor->lookup[i];
      if ((sensor->coeff == NTC && cur >= digit) || (sensor->coeff == PTC && cur <= digit))
      {
         //we are outside the lookup table range, return minimum
         if (0 == i) return sensor->tempMin;
         float a = sensor->coeff == NTC?cur - digit:digit - cur;
         float b = sensor->coeff == NTC?cur - last:last - cur;
         float c = sensor->step * a / b;
         float d = (int)(sensor->step * i) + sensor->tempMin;
         return d - c;
      }
      last = cur;
   }
   return sensor->tempMax;
}
