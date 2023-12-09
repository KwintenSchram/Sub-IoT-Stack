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
 * @author contact@liquibit.be
 */

#include "button.h"
#include "errors.h"
#include "hwatomic.h"
#include "hwgpio.h"
#include "platform.h"
#include "scheduler.h"
#include "stm32_common_gpio.h"
#include <debug.h>
#include <string.h>

#if PLATFORM_NUM_BUTTONS != 1
#error "PLATFORM_NUM_BUTTONS does not match the expected value. Update platform CMakeLists.txt or platform_userbutton.c"
#endif

typedef struct {
    pin_id_t button_id;
    bool last_known_state;
    bool triggered;
} button_info_t;


ubutton_callback_t button_state_changed_callback;
button_info_t buttons[PLATFORM_NUM_BUTTONS];

static void button_callback(void* arg);
static void button_task();
static uint8_t booted_button_state = 0;
static void validate_state();

__LINK_C void __ubutton_init()
{
    error_t err;
	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    buttons[0].button_id = BUTTON1_PIN;
    for (int i = 0; i < PLATFORM_NUM_BUTTONS; i++) 
	{
        hw_gpio_configure_pin_stm(buttons[i].button_id, &GPIO_InitStruct);
        err = hw_gpio_configure_interrupt(buttons[i].button_id, GPIO_FALLING_EDGE | GPIO_RISING_EDGE, &button_callback, &buttons[i].button_id);
        assert(err == SUCCESS);
    }
    // gather all button states
        for (uint8_t i = 0; i < PLATFORM_NUM_BUTTONS; i++) 
        {
            bool button_state = hw_gpio_get_in(buttons[i].button_id);
            booted_button_state += button_state << i;
            buttons[i].last_known_state = button_state;
        }
    
    sched_register_task(&button_task);
    sched_register_task(&validate_state);
    sched_post_task(&validate_state);
}

static void validate_state()
{
    for (uint8_t i = 0; i < PLATFORM_NUM_BUTTONS; i++) 
        {
            bool button_state = hw_gpio_get_in(buttons[i].button_id);
            buttons[i].last_known_state = button_state;
            buttons[i].triggered = button_state;
        }
}

__LINK_C error_t ubutton_register_callback(ubutton_callback_t desired_callback)
{
    error_t err;
    button_state_changed_callback = desired_callback;
    for (int i = 0; i < PLATFORM_NUM_BUTTONS; i++) 
	{
        err = hw_gpio_enable_interrupt(buttons[i].button_id);
        assert(err == SUCCESS);
    }
    return SUCCESS;
}

__LINK_C error_t ubutton_deregister_callback()
{
    error_t err;
    button_state_changed_callback = NULL;
    for (int i = 0; i < PLATFORM_NUM_BUTTONS; i++) 
	{
        err = hw_gpio_disable_interrupt(buttons[i].button_id);
        assert(err == SUCCESS);
    }
    return SUCCESS;
}

static void button_callback(void* arg)
{
    pin_id_t pin_id = *(pin_id_t*)arg;
    for (int i = 0; i < PLATFORM_NUM_BUTTONS; i++) 
	{
        if(buttons[i].button_id == pin_id) 
		{
            buttons[i].triggered = true;
        }
    }
	if(button_state_changed_callback)
    	sched_post_task(&button_task);
}

static void button_task()
{
    for (int i = 0; i < PLATFORM_NUM_BUTTONS; i++) 
	{
        if(buttons[i].triggered) 
		{
            uint8_t all_button_state = 0;
            buttons[i].triggered = false;
            bool previous_state = buttons[i].last_known_state;
            buttons[i].last_known_state = hw_gpio_get_in(buttons[i].button_id);

			//ensure we don't repeat the same action
            if (buttons[i].last_known_state == previous_state) 
            {
                sched_post_task(&button_task);
                return;
            }


			// gather all button states
            for (uint8_t b = 0; b < PLATFORM_NUM_BUTTONS; b++) 
			{
                all_button_state += buttons[b].last_known_state << b;
            }
			
			button_state_changed_callback(i, buttons[i].last_known_state, all_button_state);
            sched_post_task(&button_task);
            return;
        }
    }
}

buttons_state_t button_get_booted_state()
{
    return booted_button_state;
}
