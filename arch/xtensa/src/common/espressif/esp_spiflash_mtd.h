/****************************************************************************
 * arch/xtensa/src/common/espressif/esp_spiflash_mtd.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

#ifndef __ARCH_XTENSA_SRC_COMMON_ESPRESSIF_ESP_SPIFLASH_MTD_H
#define __ARCH_XTENSA_SRC_COMMON_ESPRESSIF_ESP_SPIFLASH_MTD_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>
#include <stdint.h>
#include <nuttx/mtd/mtd.h>

#ifndef __ASSEMBLY__

#undef EXTERN
#if defined(__cplusplus)
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name: esp_spiflash_mtd
 *
 * Description:
 *   Get SPI Flash MTD.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   SPI Flash MTD pointer.
 *
 ****************************************************************************/

struct mtd_dev_s *esp_spiflash_mtd(void);

/****************************************************************************
 * Name: esp_spiflash_alloc_mtdpart
 *
 * Description:
 *   Allocate an MTD partition from the SPI Flash.
 *
 * Input Parameters:
 *   mtd_offset - MTD Partition offset from the base address in SPI Flash.
 *   mtd_size   - Size for the MTD partition.
 *
 * Returned Value:
 *   SPI Flash MTD data pointer if success or NULL if fail.
 *
 ****************************************************************************/

struct mtd_dev_s *esp_spiflash_alloc_mtdpart(uint32_t mtd_offset,
                                             uint32_t mtd_size);

#ifdef __cplusplus
}
#endif
#undef EXTERN

#endif /* __ASSEMBLY__ */
#endif /* __ARCH_XTENSA_SRC_COMMON_ESPRESSIF_ESP_SPIFLASH_MTD_H */
