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
#include "AcuRev_1312_RCT.h"
#include "hwuart.h"
#include "mmodbus.h"
#include "math.h"

#define MODBUS_MAX_RETRIES 10


#ifdef true
#include "log.h"
#define DPRINT(...) log_print_string(__VA_ARGS__)
#define DPRINT_DATA(...) log_print_data(__VA_ARGS__)
#else
#define DPRINT(...)
#define DPRINT_DATA(...)
#endif

static uart_handle_t* uart;

#define password 0

#define device_address 1
#define modbus_timeout 1000
#define Total_Real_Energy_Imported_register 4211 // size 32 bit
#define	Real_Energy_Sunpec_Scale_Factor_register 4219 // -3 - 0
#define Total_Apparent_Energy_Imported_register 4228 // size 32 bit
#define	Apparent_Energy_Sunspec_Scale_Factor_register 4236 // -3 - 0


void acurev_1312_rct_init()
{
    uart = uart_init(0, 19200, 0);
    uart_enable(uart);
    uart_set_rx_interrupt_callback(uart, &modbus_callback_stack);
    uart_rx_interrupt_enable(uart);
    
    mmodbus_init(modbus_timeout);
    mmodbus_set32bitOrder(MModBus_32bitOrder_CDAB);
    DPRINT("acurev inited");
}

bool acurev_get_real_energy(int64_t *real_energy)
{
    uint32_t data;
    uint16_t scale;
    bool success = false;
    uint8_t retry_counter = 0;
    while(!success && retry_counter <= MODBUS_MAX_RETRIES)
    {
        success = mmodbus_readHoldingRegister16i(device_address, Real_Energy_Sunpec_Scale_Factor_register, &scale);
        success &= mmodbus_readHoldingRegister32i(device_address, Total_Real_Energy_Imported_register, &data);
        *real_energy = (int64_t)((int32_t)data * pow(10, (int16_t)scale + 3)); //convert to watt
        retry_counter++;
    }
    return success;
}

bool acurev_get_apparent_energy(int64_t *apparent_energy)
{
    uint32_t data;
    uint16_t scale;
    bool success = false;
    uint8_t retry_counter = 0;
    while(!success && retry_counter <= MODBUS_MAX_RETRIES)
    {
        success = mmodbus_readHoldingRegister16i(device_address, Apparent_Energy_Sunspec_Scale_Factor_register, &scale);
        success &= mmodbus_readHoldingRegister32i(device_address, Total_Apparent_Energy_Imported_register, &data);
        *apparent_energy = (int32_t)data * pow(10, (int16_t)scale + 3); //convert to watt
        retry_counter++;
    }

    return success;
}

