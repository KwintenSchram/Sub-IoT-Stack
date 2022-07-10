#ifndef __HDC1080DM__H
#define __HDC1080DM__H

#include "hwi2c.h"
#include <stdio.h>
#include "errors.h"

typedef enum {
	HDC1080_RESOLUTION_8BIT,
	HDC1080_RESOLUTION_11BIT,
	HDC1080_RESOLUTION_14BIT,
} HDC1080_measurement_resolution;

typedef union {
	uint8_t rawData;
	struct {
		uint8_t HumidityMeasurementResolution : 2;
		uint8_t TemperatureMeasurementResolution : 1;
		uint8_t BatteryStatus : 1;
		uint8_t ModeOfAcquisition : 1;
		uint8_t Heater : 1;
		uint8_t ReservedAgain : 1;
		uint8_t SoftwareReset : 1;
	};
} HDC1080_config_register_t;

error_t HDC1080DM_init(i2c_handle_t* i2c_handle);
error_t HDC1080DM_write_config_register(HDC1080_config_register_t reg);
error_t HDC1080DM_set_resolution(HDC1080_measurement_resolution humidity, HDC1080_measurement_resolution temperature);
error_t HDC1080DM_heat_up(uint8_t seconds);
error_t HDC1080DM_read_temperature(float* parsed_temperature);
error_t HDC1080DM_read_humidity(float* parsed_humidity);


#endif