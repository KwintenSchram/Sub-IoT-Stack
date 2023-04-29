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
#include "debug.h"
#include "hwsystem.h"
#include "log.h"
#include "platform.h"
#include "stm32l0xx_hal_adc.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "adc_stuff.h"
#include "stm32_common_gpio.h"

#define PORT_BASE(pin)  ((GPIO_TypeDef*)(pin & ~0x0F))
ADC_HandleTypeDef hadc;

uint16_t current_voltage = 0;

static void MX_ADC_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = { 0 };
    hadc.Instance = ADC1;
    hadc.Init.OversamplingMode = ADC_OVERSAMPLING_RATIO_256;
    hadc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
    hadc.Init.Resolution = ADC_RESOLUTION_12B;
    hadc.Init.SamplingTime = ADC_SAMPLETIME_160CYCLES_5;
    hadc.Init.ScanConvMode = ADC_SCAN_DIRECTION_FORWARD;
    hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc.Init.ContinuousConvMode = DISABLE;
    hadc.Init.DiscontinuousConvMode = DISABLE;
    hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc.Init.DMAContinuousRequests = DISABLE;
    hadc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadc.Init.Overrun = ADC_OVR_DATA_PRESERVED;
    hadc.Init.LowPowerAutoWait = DISABLE;
    hadc.Init.LowPowerFrequencyMode = DISABLE;
    hadc.Init.LowPowerAutoPowerOff = DISABLE;
    if (HAL_ADC_Init(&hadc) != HAL_OK) {
        log_print_string("error");
    }
    sConfig.Channel = BATTERY_VOLTAGE_ADC_CHANNEL;
    sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
    if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK) {
        log_print_string("error");
    }
}

void HAL_ADC_MspInit(ADC_HandleTypeDef* hadc_local)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };
    if (hadc_local->Instance == ADC1) {
        __HAL_RCC_ADC1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        GPIO_InitStruct.Pin = 1 << GPIO_PIN(BATTERY_VOLTAGE_PIN);
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(PORT_BASE(BATTERY_VOLTAGE_PIN), &GPIO_InitStruct);
    }
}

void HAL_ADC_MspDeInit(ADC_HandleTypeDef* hadc_local)
{
    if (hadc_local->Instance == ADC1) {
        __HAL_RCC_ADC1_CLK_DISABLE();
        HAL_GPIO_DeInit(PORT_BASE(BATTERY_VOLTAGE_PIN), 1 << GPIO_PIN(BATTERY_VOLTAGE_PIN));
    }
}

void adc_stuff_init()
{
    MX_ADC_Init();
    update_battery_voltage();
}

void update_battery_voltage()
{
    float battery_voltage;
    HAL_ADC_Start(&hadc);
    HAL_ADC_PollForConversion(&hadc, HAL_MAX_DELAY);

    //Vbat = measured voltage / (R1/R1+R2)
    // measured voltage = (ADC_value / (2^12) ) * VDD
    // Vbat = (ADC_value / (2^12) ) * VDD  * (R1+R2/R1)

    // VDD=2700  -   (R1+R2/R1) = ( (10+6.04) /10)
    // (1/4096)*2700*(16.04/10) = 1.05732421876

    battery_voltage = (float) HAL_ADC_GetValue(&hadc) * 1.05732421876; 

    HAL_ADC_Stop(&hadc);
    current_voltage = (uint16_t)(round(battery_voltage));
}

uint16_t get_battery_voltage() { return current_voltage; }