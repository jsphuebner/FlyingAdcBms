/*
 * This file is part of the FlyingAdcBms project.
 *
 * Copyright (C) 2022 Johannes Huebner <dev@johanneshuebner.com>
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
#define VER 0.22.B

/* Entries must be ordered as follows:
   1. Saveable parameters (id != 0)
   2. Temporary parameters (id = 0)
   3. Display values
 */
//Next param id (increase when adding new parameter!): 54
//Next value Id: 2104
/*              category     name         unit       min     max     default id */
#define PARAM_LIST \
    PARAM_ENTRY(CAT_BMS,     gain,        "mV/dig",  1,      1000,   586,    3   ) \
    PARAM_ENTRY(CAT_BMS,     correction0, "ppm",     -10000, 10000,  -1250,  14  ) \
    PARAM_ENTRY(CAT_BMS,     correction1, "ppm",     -10000, 10000,  1500,   15  ) \
    PARAM_ENTRY(CAT_BMS,     correction15,"ppm",     -10000, 10000,  1000,   16  ) \
    PARAM_ENTRY(CAT_BMS,     numchan,     "",        1,      16,     16,     4   ) \
    PARAM_ENTRY(CAT_BMS,     balmode,     BALMODE,   0,      3,      0,      5   ) \
    PARAM_ENTRY(CAT_BMS,     ubalance,    "mV",      0,      4500,   4500,   30  ) \
    PARAM_ENTRY(CAT_BMS,     idlewait,    "s",       0,      100000, 60,     12  ) \
    PARAM_ENTRY(CAT_BAT,     dischargemax,"A",       1,      2047,   200,    32  ) \
    PARAM_ENTRY(CAT_BAT,     nomcap,      "Ah",      0,      1000,   100,    9   ) \
    PARAM_ENTRY(CAT_BAT,     icc1,        "A",       1,      2000,   50,     43  ) \
    PARAM_ENTRY(CAT_BAT,     icc2,        "A",       1,      2000,   30,     44  ) \
    PARAM_ENTRY(CAT_BAT,     icc3,        "A",       1,      2000,   20,     45  ) \
    PARAM_ENTRY(CAT_BAT,     ucv1,        "mV",      3000,   4500,   3900,   46  ) \
    PARAM_ENTRY(CAT_BAT,     ucv2,        "mV",      3000,   4500,   4000,   47  ) \
    PARAM_ENTRY(CAT_BAT,     ucellmax,    "mV",      1000,   4500,   4200,   29  ) \
    PARAM_ENTRY(CAT_BAT,     ucellmin,    "mV",      1000,   4500,   3300,   28  ) \
    PARAM_ENTRY(CAT_BAT,     ucell0soc,   "mV",      2000,   4500,   3300,   17  ) \
    PARAM_ENTRY(CAT_BAT,     ucell10soc,  "mV",      2000,   4500,   3400,   18  ) \
    PARAM_ENTRY(CAT_BAT,     ucell20soc,  "mV",      2000,   4500,   3450,   19  ) \
    PARAM_ENTRY(CAT_BAT,     ucell30soc,  "mV",      2000,   4500,   3500,   20  ) \
    PARAM_ENTRY(CAT_BAT,     ucell40soc,  "mV",      2000,   4500,   3560,   21  ) \
    PARAM_ENTRY(CAT_BAT,     ucell50soc,  "mV",      2000,   4500,   3600,   22  ) \
    PARAM_ENTRY(CAT_BAT,     ucell60soc,  "mV",      2000,   4500,   3700,   23  ) \
    PARAM_ENTRY(CAT_BAT,     ucell70soc,  "mV",      2000,   4500,   3800,   24  ) \
    PARAM_ENTRY(CAT_BAT,     ucell80soc,  "mV",      2000,   4500,   4000,   25  ) \
    PARAM_ENTRY(CAT_BAT,     ucell90soc,  "mV",      2000,   4500,   4100,   26  ) \
    PARAM_ENTRY(CAT_BAT,     ucell100soc, "mV",      2000,   4500,   4200,   27  ) \
    PARAM_ENTRY(CAT_BAT,     sohpreset,   "%",       10,     100,    100,    53  ) \
    PARAM_ENTRY(CAT_SENS,    idcgain,     "dig/A",  -1000,   1000,   10,     6   ) \
    PARAM_ENTRY(CAT_SENS,    idcofs,      "dig",    -4095,   4095,   0,      7   ) \
    PARAM_ENTRY(CAT_SENS,    idcmode,     IDCMODES,  0,      3,      0,      8   ) \
    PARAM_ENTRY(CAT_SENS,    tempsns,     TEMPSNS,   0,      3,     -1,      52  ) \
    PARAM_ENTRY(CAT_SENS,    tempres,     "Ohm",     10,     500000, 10000,  50  ) \
    PARAM_ENTRY(CAT_SENS,    tempbeta,    "",        1,      100000, 3900,   51  ) \
    PARAM_ENTRY(CAT_COMM,    pdobase,     "",        0,      2047,   500,    10  ) \
    PARAM_ENTRY(CAT_COMM,    sdobase,     "",        0,      63,     10,     11  ) \
    TESTP_ENTRY(CAT_TEST,    enable,      OFFON,     0,      1,      1,      48   ) \
    TESTP_ENTRY(CAT_TEST,    testchan,    "",        -1,     15,     -1,     49   ) \
    VALUE_ENTRY(version,     VERSTR, 2001 ) \
    VALUE_ENTRY(opmode,      OPMODES,2000 ) \
    VALUE_ENTRY(lasterr,     errorListString,2101 ) \
    VALUE_ENTRY(errchan,     "",     2102 ) \
    VALUE_ENTRY(modaddr,     "",     2045 ) \
    VALUE_ENTRY(modnum,      "",     2046 ) \
    VALUE_ENTRY(totalcells,  "",     2074 ) \
    VALUE_ENTRY(counter,     "",     2076 ) \
    VALUE_ENTRY(uptime,      "s",    2103 ) \
    VALUE_ENTRY(chargein,    "As",   2040 ) \
    VALUE_ENTRY(chargeout,   "As",   2041 ) \
    VALUE_ENTRY(soc,         "%",    2071 ) \
    VALUE_ENTRY(soh,         "%",    2086 ) \
    VALUE_ENTRY(chargelim,   "A",    2072 ) \
    VALUE_ENTRY(dischargelim,"A",    2073 ) \
    VALUE_ENTRY(idc,         "A",    2042 ) \
    VALUE_ENTRY(idcavg,      "A",    2043 ) \
    VALUE_ENTRY(power,       "W",    2075 ) \
    VALUE_ENTRY(tempmin,     "°C",   2044 ) \
    VALUE_ENTRY(tempmax,     "°C",   2077 ) \
    VALUE_ENTRY(uavg,        "mV",   2002 ) \
    VALUE_ENTRY(umin,        "mV",   2003 ) \
    VALUE_ENTRY(umax,        "mV",   2004 ) \
    VALUE_ENTRY(udelta,      "mV",   2005 ) \
    VALUE_ENTRY(utotal,      "mV",   2039 ) \
    VALUE_ENTRY(u0,          "mV",   2006 ) \
    VALUE_ENTRY(u1,          "mV",   2007 ) \
    VALUE_ENTRY(u2,          "mV",   2008 ) \
    VALUE_ENTRY(u3,          "mV",   2009 ) \
    VALUE_ENTRY(u4,          "mV",   2010 ) \
    VALUE_ENTRY(u5,          "mV",   2011 ) \
    VALUE_ENTRY(u6,          "mV",   2012 ) \
    VALUE_ENTRY(u7,          "mV",   2013 ) \
    VALUE_ENTRY(u8,          "mV",   2014 ) \
    VALUE_ENTRY(u9,          "mV",   2015 ) \
    VALUE_ENTRY(u10,         "mV",   2016 ) \
    VALUE_ENTRY(u11,         "mV",   2017 ) \
    VALUE_ENTRY(u12,         "mV",   2018 ) \
    VALUE_ENTRY(u13,         "mV",   2019 ) \
    VALUE_ENTRY(u14,         "mV",   2020 ) \
    VALUE_ENTRY(u15,         "mV",   2021 ) \
    VALUE_ENTRY(uavg0,       "mV",   2047 ) \
    VALUE_ENTRY(umin0,       "mV",   2048 ) \
    VALUE_ENTRY(umax0,       "mV",   2049 ) \
    VALUE_ENTRY(tempmin0,    "°C",   2078 ) \
    VALUE_ENTRY(tempmax0,    "°C",   2079 ) \
    VALUE_ENTRY(uavg1,       "mV",   2050 ) \
    VALUE_ENTRY(umin1,       "mV",   2051 ) \
    VALUE_ENTRY(umax1,       "mV",   2052 ) \
    VALUE_ENTRY(tempmin1,    "°C",   2087 ) \
    VALUE_ENTRY(tempmax1,    "°C",   2088 ) \
    VALUE_ENTRY(uavg2,       "mV",   2053 ) \
    VALUE_ENTRY(umin2,       "mV",   2054 ) \
    VALUE_ENTRY(umax2,       "mV",   2055 ) \
    VALUE_ENTRY(tempmin2,    "°C",   2089 ) \
    VALUE_ENTRY(tempmax2,    "°C",   2090 ) \
    VALUE_ENTRY(uavg3,       "mV",   2056 ) \
    VALUE_ENTRY(umin3,       "mV",   2057 ) \
    VALUE_ENTRY(umax3,       "mV",   2058 ) \
    VALUE_ENTRY(tempmin3,    "°C",   2091 ) \
    VALUE_ENTRY(tempmax3,    "°C",   2092 ) \
    VALUE_ENTRY(uavg4,       "mV",   2059 ) \
    VALUE_ENTRY(umin4,       "mV",   2060 ) \
    VALUE_ENTRY(umax4,       "mV",   2061 ) \
    VALUE_ENTRY(tempmin4,    "°C",   2093 ) \
    VALUE_ENTRY(tempmax4,    "°C",   2094 ) \
    VALUE_ENTRY(uavg5,       "mV",   2062 ) \
    VALUE_ENTRY(umin5,       "mV",   2063 ) \
    VALUE_ENTRY(umax5,       "mV",   2064 ) \
    VALUE_ENTRY(tempmin5,    "°C",   2095 ) \
    VALUE_ENTRY(tempmax5,    "°C",   2096 ) \
    VALUE_ENTRY(uavg6,       "mV",   2065 ) \
    VALUE_ENTRY(umin6,       "mV",   2066 ) \
    VALUE_ENTRY(umax6,       "mV",   2067 ) \
    VALUE_ENTRY(tempmin6,    "°C",   2097 ) \
    VALUE_ENTRY(tempmax6,    "°C",   2098 ) \
    VALUE_ENTRY(uavg7,       "mV",   2068 ) \
    VALUE_ENTRY(umin7,       "mV",   2069 ) \
    VALUE_ENTRY(umax7,       "mV",   2070 ) \
    VALUE_ENTRY(tempmin7,    "°C",   2099 ) \
    VALUE_ENTRY(tempmax7,    "°C",   2100 ) \
    VALUE_ENTRY(u0cmd,       BAL,    2022 ) \
    VALUE_ENTRY(u1cmd,       BAL,    2023 ) \
    VALUE_ENTRY(u2cmd,       BAL,    2024 ) \
    VALUE_ENTRY(u3cmd,       BAL,    2025 ) \
    VALUE_ENTRY(u4cmd,       BAL,    2026 ) \
    VALUE_ENTRY(u5cmd,       BAL,    2027 ) \
    VALUE_ENTRY(u6cmd,       BAL,    2028 ) \
    VALUE_ENTRY(u7cmd,       BAL,    2029 ) \
    VALUE_ENTRY(u8cmd,       BAL,    2030 ) \
    VALUE_ENTRY(u9cmd,       BAL,    2031 ) \
    VALUE_ENTRY(u10cmd,      BAL,    2032 ) \
    VALUE_ENTRY(u11cmd,      BAL,    2033 ) \
    VALUE_ENTRY(u12cmd,      BAL,    2034 ) \
    VALUE_ENTRY(u13cmd,      BAL,    2035 ) \
    VALUE_ENTRY(u14cmd,      BAL,    2036 ) \
    VALUE_ENTRY(u15cmd,      BAL,    2037 ) \
    VALUE_ENTRY(cpuload,     "%",    2038 )


/***** Enum String definitions *****/
#define OPMODES      "0=Boot, 1=GetAddr, 2=SetAddr, 3=ReqInfo, 4=RecvInfo, 5=Init, 6=SelfTest, 7=Run, 8=Idle, 9=Error"
#define OFFON        "0=Off, 1=On"
#define BALMODE      "0=Off, 1=Additive, 2=Dissipative, 3=Both"
#define BAL          "0=None, 1=Discharge, 2=ChargePos, 3=ChargeNeg"
#define IDCMODES     "0=Off, 1=AdcSingle, 2=AdcDifferential, 3=IsaCan"
#define TEMPSNS      "0=None, 1=Chan1, 2=Chan2, 3=Both"
#define CAT_TEST     "Testing"
#define CAT_BMS      "BMS"
#define CAT_SENS     "Sensor setup"
#define CAT_COMM     "Communication"
#define CAT_BAT      "Battery Characteristics"
#define CAT_LIM      "Battery Limits"

#define VERSTR STRINGIFY(4=VER)

/***** enums ******/

enum
{
   IDC_OFF, IDC_SINGLE, IDC_DIFFERENTIAL, IDC_ISACAN
};

enum _canspeeds
{
   CAN_PERIOD_100MS = 0,
   CAN_PERIOD_10MS,
   CAN_PERIOD_LAST
};

enum _balmode
{
   BAL_OFF = 0,
   BAL_ADD = 1,
   BAL_DIS = 2,
   BAL_BOTH = 3
};


//Generated enum-string for possible errors
extern const char* errorListString;

