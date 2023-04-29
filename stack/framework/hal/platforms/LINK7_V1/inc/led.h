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

#ifndef __PLATFORM_LED_H_
#define __PLATFORM_LED_H_

#include "hwleds.h"
#include "timer.h"

#define FLASH_ON_DURATION TIMER_TICKS_PER_SEC * 0.1
#define FLASH_OFF_DURATION TIMER_TICKS_PER_SEC * 0.2

bool led_init(void);
void led_flash(uint8_t flash_times);

#endif
