/****************************************************************************
 * arch/arm/src/efm32/hardware/efm32tg_memorymap.h
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2014 Silicon Laboratories, Inc.
 * SPDX-FileCopyrightText: 2014 Pierre-noel Bouteville . All rights reserved.
 * SPDX-FileContributor: Pierre-noel Bouteville <pnb990@gmail.com>
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * DISCLAIMER OF WARRANTY/LIMITATION OF REMEDIES: Silicon Laboratories, Inc.
 * has no obligation to support this Software. Silicon Laboratories, Inc. is
 * providing the Software "AS IS", with no express or implied warranties of
 * any kind, including, but not limited to, any implied warranties of
 * merchantability or fitness for any particular purpose or warranties
 * against infringement of any proprietary rights of a third party.
 *
 * Silicon Laboratories, Inc. will not be liable for any consequential,
 * incidental, or special damages, or any other relief, or for any claim by
 * any third party, arising from your use of this Software.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#ifndef __ARCH_ARM_SRC_EFM32_HARDWARE_EFM3TG_MEMORYMAP_H
#define __ARCH_ARM_SRC_EFM32_HARDWARE_EFM3TG_MEMORYMAP_H

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Memory Base addresses */

#define EFM32_FLASH_MEM_BASE       0x00000000 /* FLASH base address */
#  define EFM32_FLASH_MEM_BITS     0x00000028 /* FLASH used bits */
#define EFM32_RAM_CODE_MEM_BASE    0x10000000 /* RAM_CODE base address */
#  define EFM32_RAM_CODE_MEM_BITS  0x00000014 /* RAM_CODE used bits */
#define EFM32_RAM_MEM_BASE         0x20000000 /* RAM base address */
#  define EFM32_RAM_MEM_BITS       0x00000018 /* RAM used bits */
#define EFM32_PER_MEM_BASE         0x40000000 /* PER base address */
#  define EFM32_PER_MEM_BITS       0x00000020 /* PER used bits */
#define EFM32_AES_MEM_BASE         0x400e0000 /* AES base address */
#  define EFM32_AES_MEM_BITS       0x00000010 /* AES used bits */

/* Bit banding area */

#define EFM32_BITBAND_PER_BASE     0x42000000 /* Peripheral Address Space bit-band area */
#define EFM32_BITBAND_RAM_BASE     0x22000000 /* SRAM Address Space bit-band area */

/* Flash and SRAM Addresses */

#define EFM32_FLASH_BASE           0x00000000 /* Flash Base Address */
#define EFM32_SRAM_BASE            0x20000000 /* SRAM Base Address */

/* Peripheral base address */

#define EFM32_VCMP_BASE            0x40000000 /* VCMP base address */
#define EFM32_ACMP0_BASE           0x40001000 /* ACMP0 base address */
#define EFM32_ACMP1_BASE           0x40001400 /* ACMP1 base address */
#define EFM32_ADC0_BASE            0x40002000 /* ADC0 base address */
#define EFM32_DAC0_BASE            0x40004000 /* DAC0 base address */
#define EFM32_GPIO_BASE            0x40006000 /* GPIO base address */
#define EFM32_I2C0_BASE            0x4000a000 /* I2C0 base address */
#define EFM32_USART0_BASE          0x4000c000 /* USART0 base address */
#define EFM32_USART1_BASE          0x4000c400 /* USART1 base address */
#define EFM32_TIMER0_BASE          0x40010000 /* TIMER0 base address */
#define EFM32_TIMER1_BASE          0x40010400 /* TIMER1 base address */
#define EFM32_RTC_BASE             0x40080000 /* RTC base address */
#define EFM32_LEUART0_BASE         0x40084000 /* LEUART0 base address */
#define EFM32_LETIMER0_BASE        0x40082000 /* LETIMER0 base address */
#define EFM32_PCNT0_BASE           0x40086000 /* PCNT0 base address */
#define EFM32_WDOG_BASE            0x40088000 /* WDOG base address */
#define EFM32_LCD_BASE             0x4008a000 /* LCD base address */
#define EFM32_LESENSE_BASE         0x4008c000 /* LESENSE base address */
#define EFM32_MSC_BASE             0x400c0000 /* MSC base address */
#define EFM32_DMA_BASE             0x400c2000 /* DMA base address */
#define EFM32_CMU_BASE             0x400c8000 /* CMU base address */
#define EFM32_EMU_BASE             0x400c6000 /* EMU base address */
#define EFM32_RMU_BASE             0x400ca000 /* RMU base address */
#define EFM32_PRS_BASE             0x400cc000 /* PRS base address */
#define EFM32_AES_BASE             0x400e0000 /* AES base address */

/* ROM specific region */

#define EFM32_ROMTABLE_BASE        0xe00fffd0 /* ROMTABLE base address */

/* Flash specific region */

#define EFM32_USERDATA_BASE        0x0fe00000 /* User data page base address */
#define EFM32_LOCKBITS_BASE        0x0fe04000 /* Lock-bits page base address */
#define EFM32_CALIBRATE_BASE       0x0fe08000 /* CALIBRATE base address */
#define EFM32_DEVINFO_BASE         0x0fe081b0 /* DEVINFO base address */

#endif /* __ARCH_ARM_SRC_EFM32_HARDWARE_EFM3TG_MEMORYMAP_H */
