#ifndef STATE_MACHINE_FILE_H
#define STATE_MACHINE_FILE_H

#include "errors.h"
#include "stdint.h"


typedef enum {
    BOOTED_STATE,
    OPERATIONAL_STATE,
    SENSOR_CONFIGURATION_STATE,
    INTERVAL_CONFIGURATION_STATE,
    TEST_STATE,
    TRANSPORT_STATE,
    LIGHT_DETECTION_CONFIGURATION_STATE,
} APP_STATE_t;

error_t state_machine_file_initialize();
uint8_t state_machine_file_switch_state(APP_STATE_t state);

#endif