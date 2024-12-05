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
#ifndef __NETWORK_MANAGER_H
#define __NETWORK_MANAGER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef enum {
    NETWORK_MANAGER_IDLE = 0,
    NETWORK_MANAGER_READY = 1,
    NETWORK_MANAGER_TRANSMITTING = 2,
} network_state_t;

typedef void (*last_transmit_completed_callback)(bool success);

void network_manager_init(last_transmit_completed_callback last_transmit_completed_cb);
error_t transmit_file(uint8_t file_id, uint32_t offset, uint32_t length, uint8_t *data);
void get_network_quality(uint8_t* acks, uint8_t* nacks);
network_state_t get_network_manager_state();
void network_manager_set_tx_power(uint8_t tx_power);

#endif //__NETWORK_MANAGER_H