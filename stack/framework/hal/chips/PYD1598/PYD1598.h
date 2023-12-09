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
#ifndef __PYD1598__H
#define __PYD1598__H

#include "errors.h"
#include "hwgpio.h"
#include <stdio.h>

typedef void (*PYD1598_callback_t)(bool mask);

error_t PYD1598_init(pin_id_t data_in, pin_id_t data_out);
error_t PYD1598_set_state(bool state);
void PYD1598_register_callback(PYD1598_callback_t PYD1598_callback);
void PYD1598_set_settings(
    uint8_t filter_Source, uint8_t window_Time, uint8_t pulse_Counter, uint16_t blind_Time, uint8_t threshold);

#endif