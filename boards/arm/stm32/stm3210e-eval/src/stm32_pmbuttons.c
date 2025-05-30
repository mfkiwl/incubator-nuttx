/****************************************************************************
 * boards/arm/stm32/stm3210e-eval/src/stm32_pmbuttons.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <arch/board/board.h>
#include <nuttx/config.h>

#include <nuttx/board.h>
#include <nuttx/power/pm.h>
#include <arch/irq.h>

#include <stdbool.h>
#include <sys/param.h>
#include <debug.h>

#include "arm_internal.h"
#include "nvic.h"
#include "stm32_pwr.h"
#include "stm32_pm.h"
#include "stm3210e-eval.h"

#if defined(CONFIG_PM) && defined(CONFIG_ARCH_IDLE_CUSTOM) && defined(CONFIG_PM_BUTTONS)

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

#ifndef CONFIG_ARCH_BUTTONS
#  error "CONFIG_ARCH_BUTTONS is not defined in the configuration"
#endif

#define BUTTON_MIN   0
#ifdef CONFIG_INPUT_DJOYSTICK
#  define BUTTON_MAX 2
#else
#  define BUTTON_MAX 7
#endif

#ifndef CONFIG_PM_BUTTONS_MIN
#  define CONFIG_PM_BUTTONS_MIN BUTTON_MIN
#endif
#ifndef CONFIG_PM_BUTTONS_MAX
#  define CONFIG_PM_BUTTONS_MAX BUTTON_MAX
#endif

#if CONFIG_PM_BUTTONS_MIN > CONFIG_PM_BUTTONS_MAX
#  error "CONFIG_PM_BUTTONS_MIN > CONFIG_PM_BUTTONS_MAX"
#endif

#if CONFIG_PM_BUTTONS_MAX > BUTTON_MAX
#  error "CONFIG_PM_BUTTONS_MAX > BUTTON_MAX"
#endif

#ifndef CONFIG_ARCH_IRQBUTTONS
#  warning "CONFIG_ARCH_IRQBUTTONS is not defined in the configuration"
#endif

#ifndef CONFIG_PM_IRQBUTTONS_MIN
#  define CONFIG_PM_IRQBUTTONS_MIN CONFIG_PM_BUTTONS_MIN
#endif

#ifndef CONFIG_PM_IRQBUTTONS_MAX
#  define CONFIG_PM_IRQBUTTONS_MAX CONFIG_PM_BUTTONS_MAX
#endif

#if CONFIG_PM_IRQBUTTONS_MIN > CONFIG_PM_IRQBUTTONS_MAX
#  error "CONFIG_PM_IRQBUTTONS_MIN > CONFIG_PM_IRQBUTTONS_MAX"
#endif

#if CONFIG_PM_IRQBUTTONS_MAX > 7
#  error "CONFIG_PM_IRQBUTTONS_MAX > 7"
#endif

#ifndef CONFIG_PM_BUTTON_ACTIVITY
#  define CONFIG_PM_BUTTON_ACTIVITY 10
#endif

/* Miscellaneous Definitions ************************************************/

#define MIN_BUTTON MIN(CONFIG_PM_BUTTONS_MIN, CONFIG_PM_IRQBUTTONS_MIN)
#define MAX_BUTTON MAX(CONFIG_PM_BUTTONS_MAX, CONFIG_PM_IRQBUTTONS_MAX)

#define NUM_PMBUTTONS   (MAX_BUTTON - MIN_BUTTON + 1)
#define BUTTON_INDEX(b) ((b)-MIN_BUTTON)

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

#ifdef CONFIG_ARCH_IRQBUTTONS
/****************************************************************************
 * Name: button_handler
 *
 * Description:
 *   Handle a button wake-up interrupt
 *
 ****************************************************************************/

static int button_handler(int irq, void *context, void *arg)
{
  /* At this point the MCU should have already awakened.  The state
   * change will be handled in the IDLE loop when the system is re-awakened
   * The button interrupt handler should be totally ignorant of the PM
   * activities and should report button activity as if nothing
   * special happened.
   */

  pm_activity(PM_IDLE_DOMAIN, CONFIG_PM_BUTTON_ACTIVITY);
  return 0;
}
#endif /* CONFIG_ARCH_IRQBUTTONS */

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stm32_pmbuttons
 *
 * Description:
 *   Configure all the buttons of the STM3210e-eval board as EXTI,
 *   so any button is able to wakeup the MCU from the PM_STANDBY mode
 *
 ****************************************************************************/

void stm32_pmbuttons(void)
{
#ifdef CONFIG_ARCH_IRQBUTTONS
  int ret;
  int i;
#endif

  /* Initialize the button GPIOs */

  board_button_initialize();

#ifdef CONFIG_ARCH_IRQBUTTONS
  for (i = CONFIG_PM_IRQBUTTONS_MIN; i <= CONFIG_PM_IRQBUTTONS_MAX; i++)
    {
      ret = board_button_irq(i, button_handler, (void *)i);
      if (ret < 0)
        {
          serr("ERROR: board_button_irq failed: %d\n", ret);
        }
    }
#endif
}

#endif /* defined(CONFIG_PM) && defined(CONFIG_ARCH_IDLE_CUSTOM) && defined(CONFIG_PM_BUTTONS) */
