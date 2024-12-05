/**
 * Copyright (C) 2020 Bosch Sensortec GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus */

#include "bma400.h"


typedef struct {
    bool current_motion_state;
    uint32_t  steps;
    uint8_t current_activity_type;
} bma400_data_t;

typedef void (*bma400_data_change_callback_t)(bma400_data_t data);

int8_t bma400_interface_init(i2c_handle_t* handle);
int8_t bma400_setup_interrupts(bool step_counter, bool activity_monitor_interrupt, pin_id_t pin_id, bma400_data_change_callback_t bma400_data_change_cb);
void bma400_set_interrupt_enabled_state(bool active);

#ifdef __cplusplus
}
#endif /*__cplusplus */
