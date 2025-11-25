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
#include <libopencm3/cm3/common.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/rtc.h>
#include <libopencm3/stm32/crc.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/desig.h>
#include "hwdefs.h"
#include "hwinit.h"
#include "stm32_loader.h"
#include "my_string.h"

/**
* Start clocks of all needed peripherals
*/
void clock_setup(void)
{
   rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_HSI_64MHZ]);

   //The reset value for PRIGROUP (=0) is not actually a defined
   //value. Explicitly set 16 preemtion priorities
   SCB_AIRCR = SCB_AIRCR_VECTKEY | SCB_AIRCR_PRIGROUP_GROUP16_NOSUB;

   rcc_periph_clock_enable(RCC_GPIOA);
   rcc_periph_clock_enable(RCC_GPIOB);
   rcc_periph_clock_enable(RCC_GPIOC);
   rcc_periph_clock_enable(RCC_USART3);
   rcc_periph_clock_enable(RCC_TIM2); //Scheduler
   rcc_periph_clock_enable(RCC_DMA1);  //ADC
   rcc_periph_clock_enable(RCC_ADC1);
   rcc_periph_clock_enable(RCC_CRC);
   rcc_periph_clock_enable(RCC_CAN1); //CAN
   rcc_periph_clock_enable(RCC_AFIO); //Needed to disable JTAG!
   rcc_periph_clock_enable(RCC_SPI1); //Needed on V1 HW
}

void spi_setup()
{
   rcc_set_ppre2(RCC_CFGR_PPRE_DIV4); //slow down SPI1 interface

   spi_init_master(SPI1, SPI_CR1_BAUDRATE_FPCLK_DIV_256, SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE,
                  SPI_CR1_CPHA_CLK_TRANSITION_1, SPI_CR1_DFF_16BIT, SPI_CR1_MSBFIRST);
   spi_enable_software_slave_management(SPI1);
   spi_set_nss_high(SPI1);
   gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO5 | GPIO7);
   spi_enable(SPI1);
}

/* Some pins should never be left floating at any time
 * Since the bootloader delays firmware startup by a few 100ms
 * We need to tell it which pins we want to initialize right
 * after startup
 */
void write_bootloader_pininit()
{
   uint32_t flashSize = desig_get_flash_size();
   uint32_t pindefAddr = FLASH_BASE + flashSize * 1024 - PINDEF_BLKNUM * PINDEF_BLKSIZE;
   const struct pincommands* flashCommands = (struct pincommands*)pindefAddr;

   struct pincommands commands;

   memset32((int*)&commands, 0, PINDEF_NUMWORDS);

   //Turn off mux at startup
   commands.pindef[0].port = GPIOB;
   commands.pindef[0].pin = 255;
   commands.pindef[0].inout = PIN_OUT;
   commands.pindef[0].level = 0;

   crc_reset();
   uint32_t crc = crc_calculate_block(((uint32_t*)&commands), PINDEF_NUMWORDS);
   commands.crc = crc;

   if (commands.crc != flashCommands->crc)
   {
      flash_unlock();
      flash_erase_page(pindefAddr);

      //Write flash including crc, therefor <=
      for (uint32_t idx = 0; idx <= PINDEF_NUMWORDS; idx++)
      {
         uint32_t* pData = ((uint32_t*)&commands) + idx;
         flash_program_word(pindefAddr + idx * sizeof(uint32_t), *pData);
      }
      flash_lock();
   }
}

/**
* Enable Timer refresh and break interrupts
*/
void nvic_setup(void)
{
   nvic_enable_irq(NVIC_TIM2_IRQ); //Scheduler
   nvic_set_priority(NVIC_TIM2_IRQ, 0); //highest priority
}

void rtc_setup()
{
   //Base clock is LSI/128 = 40 kHz
   //40 kHz / (39999 + 1) = 1Hz
   rtc_auto_awake(RCC_LSI, 39999); //1s tick
}

HwRev detect_hw()
{
   #ifdef HWV1
   return HW_1X;
   #endif // HWV1

   HwRev hwrev = HW_UNKNOWN;
   //configure as input with pull-up
   gpio_set_mode(GPIOB, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, GPIO9 | GPIO10 | GPIO11);
   gpio_set(GPIOB, GPIO9 | GPIO10 | GPIO11);
   uint16_t revisionInput = gpio_get(GPIOB, GPIO9 | GPIO10 | GPIO11);
   const uint16_t rev23 = GPIO10 | GPIO11;
   const uint16_t rev24 = GPIO9 | GPIO11;
   const uint16_t revUnknown = GPIO9 | GPIO10 | GPIO11;

   if (revisionInput == revUnknown)
   {
      //We are on hardware before 2.3, none of the pins are pulled to GND
      //check if mux pins are pulled down
      gpio_set_mode(GPIOB, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, GPIO0);
      gpio_set(GPIOB, GPIO0);

      for (volatile int i = 0; i < 80000; i++);

      if (gpio_get(GPIOB, GPIO0))
      {
         //no pull up resistor, must be V2.0 or 2.1
         //V2.1 has permanent supply to RTC, check for that
         //check if rtc started from 0. It's a week indication because after power cycle it will
         //always start at 0. But on subsequent starts it will start > 0
         if (rtc_get_counter_val() > 1)
            hwrev = HW_21;
         else
            hwrev = HW_20;
      }
      else
         hwrev = HW_22;

      gpio_clear(GPIOB, GPIO0);
   }
   else if (revisionInput == rev23)
      hwrev = HW_23;
   else if (revisionInput == rev24)
      hwrev = HW_24;

   return hwrev;
}
