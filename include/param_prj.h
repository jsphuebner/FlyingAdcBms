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

/* This file contains all parameters used in your project
 * See main.cpp on how to access them.
 * If a parameters unit is of format "0=Choice, 1=AnotherChoice" etc.
 * It will be displayed as a dropdown in the web interface
 * If it is a spot value, the decimal is translated to the name, i.e. 0 becomes "Choice"
 * If the enum values are powers of two, they will be displayed as flags, example
 * "0=None, 1=Flag1, 2=Flag2, 4=Flag3, 8=Flag4" and the value is 5.
 * It means that Flag1 and Flag3 are active -> Display "Flag1 | Flag3"
 *
 * Every parameter/value has a unique ID that must never change. This is used when loading parameters
 * from flash, so even across firmware versions saved parameters in flash can always be mapped
 * back to our list here. If a new value is added, it will receive its default value
 * because it will not be found in flash.
 * The unique ID is also used in the CAN module, to be able to recover the CAN map
 * no matter which firmware version saved it to flash.
 * Make sure to keep track of your ids and avoid duplicates. Also don't re-assign
 * IDs from deleted parameters because you will end up loading some random value
 * into your new parameter!
 * IDs are 16 bit, so 65535 is the maximum
 */

 //Define a version string of your firmware here
#define VER 1.00.R

/* Entries must be ordered as follows:
   1. Saveable parameters (id != 0)
   2. Temporary parameters (id = 0)
   3. Display values
 */
//Next param id (increase when adding new parameter!): 3
//Next value Id: 2005
/*              category     name         unit       min     max     default id */
#define PARAM_LIST \
    PARAM_ENTRY(CAT_BMS,     gain,        "mV/dig",  1,      100000, 585,    3   ) \
    PARAM_ENTRY(CAT_BMS,     numchan,     "",        1,      16,     8,      4   ) \
    PARAM_ENTRY(CAT_BMS,     balance,     OFFON,     0,      1,      0,      5   ) \
    PARAM_ENTRY(CAT_TEST,    i2cout,      "dig",     0,      15,     10,     0   ) \
    VALUE_ENTRY(opmode,      OPMODES, 2000 ) \
    VALUE_ENTRY(version,     VERSTR,  2001 ) \
    VALUE_ENTRY(uavg,        "mV",   2003 ) \
    VALUE_ENTRY(umin,        "mV",   2003 ) \
    VALUE_ENTRY(umax,        "mV",   2003 ) \
    VALUE_ENTRY(udelta,      "mV",   2003 ) \
    VALUE_ENTRY(u0,          "mV",   2003 ) \
    VALUE_ENTRY(u1,          "mV",   2003 ) \
    VALUE_ENTRY(u2,          "mV",   2003 ) \
    VALUE_ENTRY(u3,          "mV",   2003 ) \
    VALUE_ENTRY(u4,          "mV",   2003 ) \
    VALUE_ENTRY(u5,          "mV",   2003 ) \
    VALUE_ENTRY(u6,          "mV",   2003 ) \
    VALUE_ENTRY(u7,          "mV",   2003 ) \
    VALUE_ENTRY(u8,          "mV",   2003 ) \
    VALUE_ENTRY(u9,          "mV",   2003 ) \
    VALUE_ENTRY(u10,         "mV",   2003 ) \
    VALUE_ENTRY(u11,         "mV",   2003 ) \
    VALUE_ENTRY(u12,         "mV",   2003 ) \
    VALUE_ENTRY(u13,         "mV",   2003 ) \
    VALUE_ENTRY(u14,         "mV",   2003 ) \
    VALUE_ENTRY(u15,         "mV",   2003 ) \
    VALUE_ENTRY(u0cmd,       BAL,   2003 ) \
    VALUE_ENTRY(u1cmd,       BAL,   2003 ) \
    VALUE_ENTRY(u2cmd,       BAL,   2003 ) \
    VALUE_ENTRY(u3cmd,       BAL,   2003 ) \
    VALUE_ENTRY(u4cmd,       BAL,   2003 ) \
    VALUE_ENTRY(u5cmd,       BAL,   2003 ) \
    VALUE_ENTRY(u6cmd,       BAL,   2003 ) \
    VALUE_ENTRY(u7cmd,       BAL,   2003 ) \
    VALUE_ENTRY(u8cmd,       BAL,   2003 ) \
    VALUE_ENTRY(u9cmd,       BAL,   2003 ) \
    VALUE_ENTRY(u10cmd,      BAL,   2003 ) \
    VALUE_ENTRY(u11cmd,      BAL,   2003 ) \
    VALUE_ENTRY(u12cmd,      BAL,   2003 ) \
    VALUE_ENTRY(u13cmd,      BAL,   2003 ) \
    VALUE_ENTRY(u14cmd,      BAL,   2003 ) \
    VALUE_ENTRY(u15cmd,      BAL,   2003 ) \
    VALUE_ENTRY(cpuload,     "%",    2004 )


/***** Enum String definitions *****/
#define OPMODES      "0=Off, 1=Run"
#define CANSPEEDS    "0=125k, 1=250k, 2=500k, 3=800k, 4=1M"
#define CANPERIODS   "0=100ms, 1=10ms"
#define OFFON        "0=Off, 1=On"
#define BAL          "0=None, 1=Discharge, 2=ChargePos, 3=ChargeNeg"
#define CAT_TEST     "Testing"
#define CAT_BMS      "BMS"

#define VERSTR STRINGIFY(4=VER-name)

/***** enums ******/


enum _canspeeds
{
   CAN_PERIOD_100MS = 0,
   CAN_PERIOD_10MS,
   CAN_PERIOD_LAST
};

enum _modes
{
   MOD_OFF = 0,
   MOD_RUN,
   MOD_LAST
};

//Generated enum-string for possible errors
extern const char* errorListString;

