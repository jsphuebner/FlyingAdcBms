/*
 * This file is part of the FlyingAdcBms project.
 *
 * Copyright (C) 2023 Johannes Huebner <openinverter.org>
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
#include <libopencm3/stm32/gpio.h>
#include "flyingadcbms.h"
#include "digio.h"

#define I2C_DELAY       30
#define READ            true
#define WRITE           false
#define ADC_ADDR        0x68
#define DIO_ADDR        0x41
//ADC configuration register defines (only those we need)
#define ADC_START       0x80
#define ADC_RATE_240SPS 0x0
#define ADC_RATE_60SPS  0x4
#define ADC_RATE_15SPS  0x8
//Mux control words
#define MUX_OFF         0x0080
#define MUX_SELECT      0x80C0

static void SendRecvI2COverSPI(uint8_t address, bool read, uint8_t* data, uint8_t len);

uint8_t FlyingAdcBms::selectedChannel = 0;
uint8_t FlyingAdcBms::previousChannel = 0;
static bool lock = false;

void FlyingAdcBms::Init()
{
   gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, 255);
}

void FlyingAdcBms::MuxOff()
{
   //Turn off mux
   gpio_clear(GPIOB, 255);
   SetBalancing(BAL_OFF);
}

void FlyingAdcBms::SelectChannel(uint8_t channel)
{
   uint8_t data[2];

   selectedChannel = channel;
   //Turn off all channels
   gpio_clear(GPIOB, 255);

   //This creates some delay and should be done anyway.
   data[0] = 0x3; //output port register
   data[1] = 0x0; //All pins as output
   SendRecvI2COverSPI(DIO_ADDR, WRITE, data, 2);

   if (channel == 15) //special case
   {
      //Turn on G16 via GPIOB3 and G15 via Multiplexer
      gpio_set(GPIOB, GPIO3 | GPIO4 | GPIO5 | GPIO6 | GPIO7);
   }
   else if (channel & 1) //odd channel
   {
      //Example Chan9: turn on G10 (=even mux word 5) and G9 (odd mux word 4)
      uint8_t evenMuxWord = channel / 2 + 1;
      uint8_t oddMuxWord = (channel / 2) << 4;
      gpio_set(GPIOB, evenMuxWord | oddMuxWord | GPIO7);
   }
   else //even channel
   {
      //Example Chan8: turn on G8 (=even mux word 4) and G9 (odd mux word 4)
      uint8_t evenMuxWord = channel / 2;
      uint8_t oddMuxWord = evenMuxWord << 4;
      gpio_set(GPIOB, evenMuxWord | oddMuxWord | GPIO7);
   }
}

void FlyingAdcBms::StartAdc()
{
   uint8_t byte = ADC_START | ADC_RATE_60SPS; //Start in manual mode with 14 bit/60 SPS resolution
   SendRecvI2COverSPI(ADC_ADDR, WRITE, &byte, 1);
   previousChannel = selectedChannel; //now we can switch the mux and still read the correct result
}

float FlyingAdcBms::GetResult(float gain)
{
   uint8_t data[3];
   SendRecvI2COverSPI(ADC_ADDR, READ, data, 3);
   int32_t adc = (((int16_t)(data[0] << 8)) + data[1]);
   float result = adc;
   //Odd channels are connected to ADC with reversed polarity
   if (previousChannel & 1) result = -result;

   result *= gain;

   return result;
}

FlyingAdcBms::BalanceStatus FlyingAdcBms::SetBalancing(BalanceCommand cmd)
{
   BalanceStatus stt = STT_OFF;
   uint8_t data[2];

   data[0] = 0x1; //output port register

   switch (cmd)
   {
   case BAL_OFF:
      //odd channel: connect UOUTP to GNDA
      //even channel: UOUTN to GNDA
      data[1] = 0xA; //selectedChannel & 1 ? 0xE : 0xB;
      break;
   case BAL_DISCHARGE:
      data[1] = 0xF; //Discharge via low side FETs
      stt = STT_DISCHARGE;
      break;
   case BAL_CHARGE:
      //odd channel: connect UOUTP to GNDA and UOUTN to VCCA
      //even channel: connect UOUTP to VCCA and UOUTN to GNDA
      data[1] = selectedChannel & 1 ? 0xC : 0x3;
      stt = selectedChannel & 1 ? STT_CHARGENEG : STT_CHARGEPOS;
   }
   SendRecvI2COverSPI(DIO_ADDR, WRITE, data, 2);

   return stt;
}

static void BitBangI2CStartAddress(uint8_t address)
{
   DigIo::i2c_do.Clear();
   for (volatile int i = 0; i < I2C_DELAY*2; i++);
   DigIo::i2c_scl.Clear();

   for (int i = 16; i >= 0; i--)
   {
      if (address & 0x80 || i < 1) DigIo::i2c_do.Set();
      else DigIo::i2c_do.Clear();
      for (volatile int j = 0; j < I2C_DELAY; j++);
      DigIo::i2c_scl.Toggle();;
      if (i & 1) address <<= 1;
   }
   for (volatile int i = 0; i < I2C_DELAY; i++);

   DigIo::i2c_do.Set();
}

static uint8_t BitBangI2CByte(uint8_t byte, bool ack)
{
   uint8_t byteRead = 0;

   DigIo::i2c_scl.Clear();
   for (volatile int i = 0; i < I2C_DELAY; i++);

   for (int i = 16; i >= 0; i--)
   {
      if (byte & 0x80 || (i < 1 && !ack)) DigIo::i2c_do.Set();
      else DigIo::i2c_do.Clear();
      for (volatile int j = 0; j < I2C_DELAY; j++);
      DigIo::i2c_scl.Toggle();;
      if (i & 1) {
         byte <<= 1;
         byteRead <<= 1;
         byteRead |= DigIo::i2c_di.Get();
      }
   }
   for (volatile int i = 0; i < I2C_DELAY; i++);

   return byteRead;
}

static void BitBangI2CStop()
{
   DigIo::i2c_scl.Clear();
   for (volatile int i = 0; i < I2C_DELAY; i++);
   DigIo::i2c_do.Clear(); //data low
   for (volatile int i = 0; i < I2C_DELAY; i++);
   DigIo::i2c_scl.Set();
   for (volatile int i = 0; i < I2C_DELAY; i++);
   DigIo::i2c_do.Set(); //data high -> STOP
   for (volatile int i = 0; i < I2C_DELAY; i++);
}

static void SendRecvI2COverSPI(uint8_t address, bool read, uint8_t* data, uint8_t len)
{
   if (lock) return;

   lock = true;

   address <<= 1;
   address |= read;
   BitBangI2CStartAddress(address);

   for (int i = 0; i < len; i++)
   {
      data[i] = BitBangI2CByte(read ? 0xFF : data[i], i != (len - 1) && read);
   }

   BitBangI2CStop();

   lock = false;
}
