/*
 * Copyright (c) 2015-2021 University of Antwerp, Aloxy NV, LiQuiBit VOF.
 *
 * This file is part of Sub-IoT.
 * See https://github.com/Sub-IoT/Sub-IoT-Stack for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* \file
 *
 *
 * @author info@liquibit.be
 */

#include "errors.h"
#include "hwgpio.h"
#include "hwleds.h"
#include "led.h"
#include "platform.h"
#include "stm32_common_gpio.h"
#include "stm32_device.h"
#include "timer.h"
#include <debug.h>

#if PLATFORM_NUM_LEDS != 2
#error PLATFORM_NUM_LEDS does not match the expected value. Update platform.h or platform_leds.c
#endif
static pin_id_t leds[PLATFORM_NUM_LEDS];

static bool flashing = false;
static bool led0_manual_mode = false;
static uint8_t remaining_flashes = 0;

static void end_flash_white();
static void __led_on_sched_off();

void __led_init()
{
    leds[0] = LED1;
    leds[1] = LED2;
    for (int i = 0; i < PLATFORM_NUM_LEDS; i++) {
        hw_gpio_clr(leds[i]);
        error_t err = hw_gpio_configure_pin(leds[i], false, GPIO_MODE_OUTPUT_PP, 0);
        assert(err == SUCCESS);
    }
}

static void __led_on_sched_off()
{
    hw_gpio_set(leds[LED_WHITE]);
    timer_post_task_delay(&end_flash_white, FLASH_ON_DURATION);
}

static void end_flash_white()
{
    if(led0_manual_mode)
        return;
    hw_gpio_clr(leds[LED_WHITE]);
    if (remaining_flashes == 0)
        flashing = false;
    else {
        remaining_flashes--;
        timer_post_task_delay(&__led_on_sched_off, FLASH_OFF_DURATION);
    }
}

bool led_init()
{
    __led_init();
    sched_register_task(&end_flash_white);
    sched_register_task(&__led_on_sched_off);
    return true;
}

void led_on(uint8_t led_nr)
{
    if (led_nr < PLATFORM_NUM_LEDS)
        hw_gpio_set(leds[led_nr]);

    if(led_nr == 0)
    {
        led0_manual_mode = true;
    }
}


void led_off(unsigned char led_nr)
{
    if (led_nr < PLATFORM_NUM_LEDS)
        hw_gpio_clr(leds[led_nr]);
    if(led_nr == 0)
    {
        led0_manual_mode = false;
    }
}

void led_set(uint8_t led_nr, bool state)
{
    if (led_nr < PLATFORM_NUM_LEDS)
        hw_gpio_set(leds[led_nr]);

    if(state)
        led_on(led_nr);
    else
        led_off(led_nr);
}

void led_toggle(unsigned char led_nr)
{
    if (led_nr < PLATFORM_NUM_LEDS)
        hw_gpio_toggle(leds[led_nr]);
}

void led_flash(uint8_t flash_times)
{
    if (flashing || led0_manual_mode)
        return;
    if (!flash_times)
        return;
    remaining_flashes = flash_times - 1;
    flashing = true;
    __led_on_sched_off();
}
