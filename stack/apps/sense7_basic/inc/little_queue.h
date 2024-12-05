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
#ifndef __LITTLE_QUEUE_H
#define __LITTLE_QUEUE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_QUEUE_ELEMENTS 20

typedef enum {
    LOW_PRIORITY = 0,
    NORMAL_PRIORITY = 1,
    TOP_PRIORITY = 2,
} queue_priority_t;

void little_queue_init();
void queue_add_file(uint8_t* file_content, uint8_t file_size, uint8_t file_id);
void little_queue_set_led_state(bool state);

#endif //__LITTLE_QUEUE_H