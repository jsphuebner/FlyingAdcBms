/*
 * This file is part of the stm32-sine project.
 *
 * Copyright (C) 2011 Johannes Huebner <dev@johanneshuebner.com>
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
#ifndef TEMP_MEAS_H_INCLUDED
#define TEMP_MEAS_H_INCLUDED

class TempMeas
{
public:
   enum Sensors
   {
      TEMP_JCURVE = 0,
      TEMP_KTY81 = 1,
      TEMP_PT1000 = 2,
      TEMP_LEAFBATT = 3,
      TEMP_LAST
   };

   static float Lookup(int digit, Sensors sensorId);
};


#ifdef __TEMP_LU_TABLES
#define LEAF_BATT \
255	, \
308	, \
388	, \
460	, \
564	, \
730	, \
856	, \
1034	, \
1201	, \
1432	, \
1773	, \
2013	, \
2327	, \
2482	, \
2568	, \
2864	, \
3103

#define JCURVE \
57	,\
76	,\
100	,\
132	,\
171	,\
220	,\
280	,\
353	,\
440	,\
544	,\
665	,\
805	,\
963	,\
1141	,\
1338	,\
1551	,\
1779	,\
2019	,\
2268	,\
2523	,\
2779	,\
3032	,\
3279	,\
3519	,\
3748	,\
3964	,\
4167

#define KTY81 \
1863	,\
1991	,\
2123	,\
2253	,\
2381	,\
2510	,\
2635	,\
2760	,\
2881	,\
2998	,\
3114	,\
3226	,\
3332	,\
3437	,\
3537	,\
3634	,\
3727	,\
3815	,\
3895	,\
3965	,\
4022


#define PT1000 \
2488, \
2560, \
2629, \
2696, \
2760, \
2821, \
2880, \
2937, \
2991, \
3044, \
3095, \
3144, \
3192, \
3238, \
3282, \
3325, \
3367, \
3407, \
3446, \
3484, \
3521

#endif

#endif // TEMP_MEAS_H_INCLUDED
