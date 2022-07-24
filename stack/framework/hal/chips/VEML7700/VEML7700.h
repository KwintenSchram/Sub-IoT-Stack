#ifndef __VEML7700__H
#define __VEML7700__H

#include "errors.h"
#include "hwi2c.h"
#include <stdio.h>

typedef enum {
    ALS_INTEGRATION_25ms = 0xc,
    ALS_INTEGRATION_50ms = 0x8,
    ALS_INTEGRATION_100ms = 0x0,
    ALS_INTEGRATION_200ms = 0x1,
    ALS_INTEGRATION_400ms = 0x2,
    ALS_INTEGRATION_800ms = 0x3
} VEML7700_ALS_INTEGRATION_TIME_t;

typedef enum {
    ALS_PERSISTENCE_1 = 0x0,
    ALS_PERSISTENCE_2 = 0x1,
    ALS_PERSISTENCE_4 = 0x2,
    ALS_PERSISTENCE_8 = 0x3
} VEML7700_ALS_PERS_PROTECT_NUMBER;

typedef enum {
    ALS_GAIN_x1 = 0x0, // x 1
    ALS_GAIN_x2 = 0x1, // x 2
    ALS_GAIN_d8 = 0x2, // x 1/8
    ALS_GAIN_d4 = 0x3
} VEML7700_ALS_GAIN_t;

typedef enum {
    ALS_POWER_MODE_1 = 0x0, //600 ms/sample (ALS_INTEGRATION_100ms)
    ALS_POWER_MODE_2 = 0x1, //1100 ms/sample (ALS_INTEGRATION_100ms)
    ALS_POWER_MODE_3 = 0x2, //2100 ms/sample (ALS_INTEGRATION_100ms)
    ALS_POWER_MODE_4 = 0x3 //4100 ms/sample (ALS_INTEGRATION_100ms)
} VEML7700_ALS_POWER_MODE;



typedef union {
    uint16_t rawData;
    struct {
        uint16_t ALS_SD : 1; // ALS shut down setting
        uint16_t ALS_INT_EN : 1; // ALS interrupt enable setting
        uint16_t Reserved3 : 2;
        uint16_t ALS_PERS : 2; // ALS persistence protect number setting
        uint16_t ALS_IT : 4; // ALS integration time setting
        uint16_t reserved2 : 1;
        uint16_t ALS_GAIN : 2; // Gain selection
        uint16_t reserved1 : 3;
    };
} VEML7700_CONFIG_REG;

error_t VEML7700_init(i2c_handle_t* i2c_handle);
error_t VEML7700_configure(VEML7700_CONFIG_REG reg);
void VEML7700_change_settings(uint8_t integration_time, uint8_t persistence_number, uint8_t gain, uint8_t power_mode);
error_t VEML7700_set_power_mode(VEML7700_ALS_POWER_MODE mode);
error_t VEML7700_read_ALS_Lux(uint16_t* raw_data, float* parsed_data);
error_t VEML7700_read_White_Lux(uint16_t* raw_data, float* parsed_data);
error_t VEML7700_set_shutdown_state(bool state);

#endif