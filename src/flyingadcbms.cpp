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
#include <libopencm3/stm32/spi.h>
#include "flyingadcbms.h"
#include "digio.h"

#define I2C_DELAY 10

static void SendRecvI2COverSPI(uint8_t address, bool read, uint8_t* data, uint8_t len);

uint8_t FlyingAdcBms::selectedChannel = 0;
uint8_t FlyingAdcBms::previousChannel = 0;

void FlyingAdcBms::SelectChannel(uint8_t channel)
{
   selectedChannel = channel;
   //Select MUX channel with deadtime insertion
   spi_xfer(SPI1, 0x80C0 | channel);
}

void FlyingAdcBms::StartAdc()
{
   uint8_t byte = 0x84; //Start in manual mode with 14 bit/60 SPS resolution
   SendRecvI2COverSPI(0x68, false, &byte, 1);
   previousChannel = selectedChannel; //now we can switch the mux and still read the correct result
}

int32_t FlyingAdcBms::GetRawResult()
{
   uint8_t data[3];
   SendRecvI2COverSPI(0x68, true, data, 3);
   int32_t result = (((int16_t)(data[0] << 8)) + data[1]);

   if (previousChannel & 1) result = -result;

   return result;
}

void FlyingAdcBms::SetBalancing(BalanceCommand cmd)
{
   uint8_t data[2] = { 0x03, 0x00 }; //direction register: all outputs

   SendRecvI2COverSPI(0x41, false, data, 2);

   data[0] = 0x1; //output port register

   switch (cmd)
   {
   case BAL_OFF:
      data[1] = 0xA; //all FETs off
      break;
   case BAL_DISCHARGE:
      data[1] = 0xF; //Discharge via low side FETs
      break;
   case BAL_CHARGE:
      //odd channel: connect UOUTP to GNDA and UOUTN to VCCA
      //even channel: connect UOUTP to VCCA and UOUTN to GNDA
      data[1] = selectedChannel & 1 ? 0xC : 0x3;
   }
   SendRecvI2COverSPI(0x41, false, data, 2);
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
}

static bool lock = false;

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
