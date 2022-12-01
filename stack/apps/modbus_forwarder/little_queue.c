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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hwlcd.h"
#include "hwleds.h"
#include "hwsystem.h"

#include "d7ap_fs.h"
#include "debug.h"
#include "log.h"
#include "scheduler.h"
#include "timer.h"

#include "alp_layer.h"
#include "d7ap.h"
#include "dae.h"

#include "platform.h"

#include "led.h"
#include "little_queue.h"
#include "network_manager.h"

#define FRAMEWORK_LITTLE_QUEUE_LOG 1
#define MAX_FILE_SIZE 10
#define MAX_RETRY_ATTEMPTS 10
#define D7_TX_POWER 17

#ifdef FRAMEWORK_LITTLE_QUEUE_LOG
#include "log.h"
#define DPRINT(...) log_print_string(__VA_ARGS__)
#define DPRINT_DATA(...) log_print_data(__VA_ARGS__)
#else
#define DPRINT(...)
#define DPRINT_DATA(...)
#endif

static uint8_t file_fifo_buffer[MAX_QUEUE_ELEMENTS * MAX_FILE_SIZE];
static uint8_t file_size_and_id_fifo_buffer[MAX_QUEUE_ELEMENTS * 2];

static fifo_t file_fifo;
static fifo_t file_size_and_id_fifo;
static uint8_t retry_counter = 0;
static bool flash_led_enabled = true;

static uint8_t transmitted_file_id = 0;
static void queue_transmit_files();

static void queue_transmit_completed(bool success)
{
    // if a file successfully got transmitted or we tried too much, remove it from the queue
    if (success || retry_counter >= MAX_RETRY_ATTEMPTS) {
        uint8_t file_id;
        uint8_t file_size;
        fifo_peek(&file_size_and_id_fifo, &file_size, 0, 1);
        fifo_peek(&file_size_and_id_fifo, &file_id, 1, 1);
        fifo_skip(&file_fifo, file_size);
        fifo_skip(&file_size_and_id_fifo, 2);
        retry_counter = 0;

        if (!success)
            log_print_error_string("file %d discarded, to many tries", file_id);
    } else
        retry_counter++;

    // TODO add backoff if !success based on #transmits
    // if there are still files in the queue, transmit the next file
    if (fifo_get_size(&file_fifo) > 0)
        timer_post_task_delay(&queue_transmit_files, 50);
    // if there are no files left in the queue and the led is enabled, we flash once to show we cleared the queue
    else if (flash_led_enabled)
        led_flash(1);
}

static void queue_transmit_files()
{
    error_t ret;
    if ((fifo_get_size(&file_fifo) <= 0) || (get_network_manager_state() != NETWORK_MANAGER_READY))
        return;

    uint8_t file_buffer[MAX_FILE_SIZE];
    uint8_t file_size;
    uint8_t file_id;
    fifo_peek(&file_size_and_id_fifo, &file_size, 0, 1);
    fifo_peek(&file_size_and_id_fifo, &file_id, 1, 1);
    fifo_peek(&file_fifo, file_buffer, 0, file_size);
    DPRINT("transmitting file %d, size %d", file_id, file_size);
    // for now, we always send files with offset 0
    ret = transmit_file(file_id, 0, file_size, file_buffer);
    DPRINT_DATA(file_buffer, file_size);
    if (ret != SUCCESS)
        log_print_string("could not send file to network manager");
}

void queue_add_file(uint8_t* file_content, uint8_t file_size, uint8_t file_id)
{
    error_t ret = fifo_put(&file_fifo, file_content, file_size);

    if (ret != SUCCESS)
        log_print_error_string("queue was full. Message not added"); // TODO replace last element with this one
    else {
        ret = fifo_put(&file_size_and_id_fifo, &file_size, 1);
        ret = fifo_put(&file_size_and_id_fifo, &file_id, 1);
    }

    if (get_network_manager_state() == NETWORK_MANAGER_READY && !timer_is_task_scheduled(&queue_transmit_files))
        sched_post_task(&queue_transmit_files);
}

/**
 * @brief Initializes the queueing and transmission process
 */
void little_queue_init()
{
    network_manager_init(&queue_transmit_completed);
    network_manager_set_tx_power(D7_TX_POWER);
    sched_register_task(&queue_transmit_files);
    fifo_init(&file_fifo, file_fifo_buffer, sizeof(file_fifo_buffer));
    fifo_init(&file_size_and_id_fifo, file_size_and_id_fifo_buffer, sizeof(file_size_and_id_fifo_buffer));
}

void little_queue_set_led_state(bool state) { flash_led_enabled = state; }