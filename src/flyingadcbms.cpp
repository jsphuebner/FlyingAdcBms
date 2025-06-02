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
#include <libopencm3/stm32/spi.h>
#include "flyingadcbms.h"
#include "digio.h"
#include "hwdefs.h"

#define READ            true
#define WRITE           false
#define ADC_ADDR        0x68
#define DIO_ADDR        0x41
//ADC configuration register defines (only those we need)
#define ADC_START       0x80
#define ADC_RATE_240SPS 0x0
#define ADC_RATE_60SPS  0x4
#define ADC_RATE_15SPS  0x8

#define HBRIDGE_DISCHARGE_VIA_LOWSIDE    0xF
#define HBRIDGE_ALL_OFF                  0xA
#define HBRIDGE_UOUTP_TO_GND_UOUTN_TO_5V 0xC
#define HBRIDGE_UOUTP_TO_5V_UOUTN_TO_GND 0x3

#define DELAY() for (volatile int _ctr = 0; _ctr < i2cdelay; _ctr++)

uint8_t FlyingAdcBms::selectedChannel = 0;
uint8_t FlyingAdcBms::previousChannel = 0;
uint8_t FlyingAdcBms::i2cdelay = 30;
static bool lock = false;


#ifdef HWV1
void FlyingAdcBms::Init()
{
   uint8_t data[] = { 0x3, 0x0 };
   SendRecvI2C(DIO_ADDR, WRITE, data, 2);
   gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO0);
}

//Mux control words
#define MUX_OFF         0x0080
#define MUX_SELECT      0x80C0

void FlyingAdcBms::MuxOff()
{
   //Turn off mux
   spi_xfer(SPI1, MUX_OFF);
   SetBalancing(BAL_OFF);
   gpio_clear(GPIOB, GPIO0);
}

void FlyingAdcBms::SelectChannel(uint8_t channel)
{
   gpio_set(GPIOB, GPIO0);
   selectedChannel = channel;
   //Select MUX channel with deadtime insertion
   spi_xfer(SPI1, MUX_SELECT | channel);
}
#else
void FlyingAdcBms::Init()
{
   uint8_t data[2] = { 0x3 /* pin mode register */, 0x0 /* All pins as output */};
   SendRecvI2C(DIO_ADDR, WRITE, data, 2);
   gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, 255);

   if (hwRev == HW_23)
      i2cdelay = 5;
}

void FlyingAdcBms::MuxOff()
{
   //Turn off mux
   gpio_clear(GPIOB, 255);
}

void FlyingAdcBms::SelectChannel(uint8_t channel)
{
   //Turn off all channels
   gpio_clear(GPIOB, 255);

   if (channel > 15) return;

   selectedChannel = channel;

   //Example Chan8:  turn on G8 (=even mux word 4) and G9 (odd mux word 4)
   //Example Chan9:  turn on G10 (=even mux word 5) and G9 (odd mux word 4)
   //Example Chan15: turn on G16 via GPIOB3 (=even mux word 8) and G15 via Decoder (odd mux word 7)
   uint8_t evenMuxWord = (channel / 2) + (channel & 1);
   uint8_t oddMuxWord = (channel / 2) << 4;
   gpio_set(GPIOB, evenMuxWord | oddMuxWord | GPIO7);
}
#endif // V1HW

void FlyingAdcBms::StartAdc()
{
   uint8_t byte = ADC_START | ADC_RATE_60SPS; //Start in manual mode with 14 bit/60 SPS resolution
   SendRecvI2C(ADC_ADDR, WRITE, &byte, 1);
   previousChannel = selectedChannel; //now we can switch the mux and still read the correct result
}

float FlyingAdcBms::GetResult()
{
   uint8_t data[3];
   SendRecvI2C(ADC_ADDR, READ, data, 3);
   int32_t adc = (((int16_t)(data[0] << 8)) + data[1]);
   float result = adc;
   //Odd channels are connected to ADC with reversed polarity
   if (previousChannel & 1) result = -result;

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
      data[1] = HBRIDGE_ALL_OFF;
      break;
   case BAL_DISCHARGE:
      data[1] = HBRIDGE_DISCHARGE_VIA_LOWSIDE;
      stt = STT_DISCHARGE;
      break;
   case BAL_CHARGE:
      //odd channel: connect UOUTP to GNDA and UOUTN to VCCA
      //even channel: connect UOUTP to VCCA and UOUTN to GNDA
      data[1] = selectedChannel & 1 ? HBRIDGE_UOUTP_TO_GND_UOUTN_TO_5V : HBRIDGE_UOUTP_TO_5V_UOUTN_TO_GND;
      stt = selectedChannel & 1 ? STT_CHARGENEG : STT_CHARGEPOS;
   }

   SendRecvI2C(DIO_ADDR, WRITE, data, 2);

   return stt;
}

void FlyingAdcBms::SendRecvI2C(uint8_t address, bool read, uint8_t* data, uint8_t len)
{
   if (lock) return;

   lock = true;

   BitBangI2CStart();

   address <<= 1;
   address |= read;

   BitBangI2CByte(address, false);

   for (int i = 0; i < len; i++)
      data[i] = BitBangI2CByte(read ? 0xFF : data[i], i != (len - 1) && read);

   BitBangI2CStop();

   lock = false;
}

void FlyingAdcBms::BitBangI2CStart()
{
   DigIo::i2c_do.Clear(); //Generate start. First SDA low, then SCL
   DELAY();
   DigIo::i2c_scl.Clear();
}

uint8_t FlyingAdcBms::BitBangI2CByte(uint8_t byte, bool ack)
{
   uint8_t byteRead = 0;

   DigIo::i2c_scl.Clear();
   DELAY();

   for (int i = 16; i >= 0; i--)
   {
      if (byte & 0x80 || (i < 1 && !ack)) DigIo::i2c_do.Set();
      else DigIo::i2c_do.Clear();
      DELAY();
      DigIo::i2c_scl.Toggle();
      if (i & 1) {
         byte <<= 1; //get next bit at falling edge
      }
      else if (i > 0)
      {
         byteRead <<= 1;
         byteRead |= DigIo::i2c_di.Get(); //Read data at rising edge
      }
   }
   DELAY();

   return byteRead;
}

void FlyingAdcBms::BitBangI2CStop()
{
   DigIo::i2c_scl.Clear();
   DELAY();
   DigIo::i2c_do.Clear(); //data low
   DELAY();
   DigIo::i2c_scl.Set();
   DELAY();
   DigIo::i2c_do.Set(); //data high -> STOP
   DELAY();
}

