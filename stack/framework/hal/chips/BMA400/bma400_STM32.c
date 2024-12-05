#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "bma400.h"
#include "log.h"
#include "errors.h"
#include "hwi2c.h"
#include "hwsystem.h"
#include "stm32_common_gpio.h"
#include "timer.h"
#include "bma400_STM32.h"

/*! Read write length varies based on user requirement */
#define READ_WRITE_LENGTH  UINT8_C(46)

/* Variable to store the device address */
static uint8_t dev_addr;
i2c_handle_t* bma400_i2c;
static struct bma400_dev bma_device_handle;
static bool bma400_inited = false;
static bool bma400_interrupt_inited = false;
static bool current_motion_state = false;
static uint8_t current_activity_type = BMA400_STILL_ACT;
static bool bma400_general_interrupt_setup_inited = false;
static bma400_data_change_callback_t bma400_data_change_callback;
static pin_id_t accelerometer_interrupt_pin;

static GPIO_InitTypeDef input_config
    = { .Mode = GPIO_MODE_IT_RISING_FALLING, .Pull = GPIO_NOPULL, .Speed = GPIO_SPEED_FREQ_LOW };

static int8_t bma400_init_device();
static void bma400_interrupt_callback(void* arg);
static void process_bma400_interrupt();
static int8_t bma400_step_counter_interrupt_init();
static void pol_activity_change();

/*!
 * @brief I2C read function 
 */
int8_t bma400_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    return !i2c_read_memory(bma400_i2c, dev_addr, reg_addr, 8, (uint8_t*) reg_data, len);
}

/*!
 * @brief I2C write function 
 */
int8_t bma400_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    return !i2c_write_memory(bma400_i2c, dev_addr, reg_addr, 8, (uint8_t*) reg_data, len);
}

/*!
 * @brief Delay function
 */
void bma400_delay_us(uint32_t period, void *intf_ptr)
{
    const uint32_t max_period = 10000; // Maximum period per hw_busy_wait call
    
    // Calculate the number of full max_period delays required
    uint32_t full_delays = period / max_period;
    
    // Calculate the remaining period after the full delays
    uint32_t remaining_period = period % max_period;
    
    // Perform full max_period delays
    for(uint32_t i = 0; i < full_delays; i++)
    {
        hw_busy_wait(max_period);
    }
    
    // Perform the remaining delay, if any
    if(remaining_period > 0)
    {
        hw_busy_wait(remaining_period);
    }
}

void bma400_check_rslt(const char api_name[], int8_t rslt)
{
    switch (rslt)
    {
        case BMA400_OK:

            /* Do nothing */
            break;
        case BMA400_E_NULL_PTR:
            log_print_error_string("Error [%d] : Null pointer\r\n", rslt);
            break;
        case BMA400_E_COM_FAIL:
            log_print_error_string("Error [%d] : Communication failure\r\n", rslt);
            break;
        case BMA400_E_INVALID_CONFIG:
            log_print_error_string("Error [%d] : Invalid configuration\r\n", rslt);
            break;
        case BMA400_E_DEV_NOT_FOUND:
            log_print_error_string("Error [%d] : Device not found\r\n", rslt);
            break;
        default:
            log_print_error_string("Error [%d] : Unknown error code\r\n", rslt);
            break;
    }
}

int8_t bma400_interface_init(i2c_handle_t* handle)
{
    int8_t rslt = BMA400_OK;

    bma400_i2c = handle;

    dev_addr = BMA400_I2C_ADDRESS_SDO_LOW;
    bma_device_handle.read = bma400_i2c_read;
    bma_device_handle.write = bma400_i2c_write;
    bma_device_handle.intf = BMA400_I2C_INTF;

    bma_device_handle.intf_ptr = &dev_addr;
    bma_device_handle.delay_us = bma400_delay_us;
    bma_device_handle.read_write_len = READ_WRITE_LENGTH;

    return rslt;
}

static int8_t bma400_init_device()
{
    if(!bma400_inited)
    {
        if(bma400_i2c == NULL)
        {
            log_print_error_string("BMA400 first call interface init");
            return BMA400_E_NULL_PTR;
        }
        int8_t rslt = 0;
        rslt = bma400_init(&bma_device_handle);
        bma400_inited = (rslt == BMA400_OK);
        rslt = bma400_soft_reset(&bma_device_handle);
    }
    return BMA400_OK;
}

static int8_t bma400_init_interrupt_pins()
{
    if(!bma400_interrupt_inited)
    {
        hw_gpio_configure_pin_stm(accelerometer_interrupt_pin, &input_config);
        hw_gpio_configure_interrupt(
            accelerometer_interrupt_pin, GPIO_RISING_EDGE|GPIO_FALLING_EDGE, &bma400_interrupt_callback, NULL);
        bma400_interrupt_inited = true;
    }

    return BMA400_OK;
}

void bma400_set_interrupt_enabled_state(bool active)
{
    if(active)
        hw_gpio_enable_interrupt(accelerometer_interrupt_pin);
    else
        hw_gpio_disable_interrupt(accelerometer_interrupt_pin);
}

static int8_t bma400_setup_general_interrupts()
{
    if(bma400_general_interrupt_setup_inited)
        return BMA400_OK;
    int8_t rslt = 0;

    struct bma400_sensor_data accel;
    struct bma400_sensor_conf accel_settin[1] = { { 0 } };

    accel_settin[0].type = BMA400_GEN1_INT;

    rslt = bma400_get_sensor_conf(accel_settin, 1, &bma_device_handle);
    bma400_check_rslt("bma400_get_sensor_conf", rslt);

    accel_settin[0].param.gen_int.gen_int_thres = 5; // gen_int_thres=5 results in 40mg
    accel_settin[0].param.gen_int.gen_int_dur = 750; // 10ms resolution (eg. gen_int_dur=5 results in 50ms)
    accel_settin[0].param.gen_int.axes_sel = BMA400_AXIS_XYZ_EN;
    accel_settin[0].param.gen_int.data_src = BMA400_DATA_SRC_ACC_FILT2;
    accel_settin[0].param.gen_int.criterion_sel = BMA400_INACTIVITY_INT;
    accel_settin[0].param.gen_int.evaluate_axes = BMA400_ALL_AXES_INT;
    accel_settin[0].param.gen_int.ref_update = BMA400_UPDATE_EVERY_TIME;
    accel_settin[0].param.gen_int.hysteresis = BMA400_HYST_48_MG;
    accel_settin[0].param.gen_int.int_thres_ref_x = 0;
    accel_settin[0].param.gen_int.int_thres_ref_y = 0;
    accel_settin[0].param.gen_int.int_thres_ref_z = 0;
    accel_settin[0].param.gen_int.int_chan = BMA400_INT_CHANNEL_2;

    /* Set the desired configurations to the sensor */
    rslt = bma400_set_sensor_conf(accel_settin, 1, &bma_device_handle);
    bma400_check_rslt("bma400_set_sensor_conf", rslt);

    struct bma400_device_conf device_settin[2] = { { 0 } };
    device_settin[0].type = BMA400_AUTOWAKEUP_INT;
    device_settin[1].type = BMA400_AUTO_LOW_POWER;

    bma400_get_device_conf(device_settin, 2, &bma_device_handle);
    bma400_check_rslt("bma400_get_device_conf", rslt);
    device_settin[0].param.wakeup.wakeup_ref_update = BMA400_UPDATE_EVERY_TIME;
    device_settin[0].param.wakeup.sample_count = BMA400_SAMPLE_COUNT_8;
    device_settin[0].param.wakeup.wakeup_axes_en = BMA400_AXIS_XYZ_EN;
    device_settin[0].param.wakeup.int_wkup_threshold = 15;
    device_settin[0].param.wakeup.int_wkup_ref_x = 0;
    device_settin[0].param.wakeup.int_wkup_ref_y = 0;
    device_settin[0].param.wakeup.int_wkup_ref_z = 0;
    device_settin[0].param.wakeup.int_chan = BMA400_INT_CHANNEL_2;

    device_settin[1].param.auto_lp.auto_low_power_trigger = BMA400_AUTO_LP_GEN1_TRIGGER;
    device_settin[1].param.auto_lp.auto_lp_timeout_threshold = 0;

    bma400_set_device_conf(device_settin, 2, &bma_device_handle);
    bma400_check_rslt("bma400_stt_device_conf", rslt);

    bma400_general_interrupt_setup_inited = true;
}

static int8_t bma400_motion_monitoring()
{
    int8_t rslt = 0;
    struct bma400_int_enable int_en[2];

    rslt = bma400_setup_general_interrupts();

    int_en[0].type = BMA400_GEN1_INT_EN;
    int_en[0].conf = BMA400_ENABLE;
    int_en[1].type = BMA400_AUTO_WAKEUP_EN;
    int_en[1].conf = BMA400_ENABLE;

    rslt = bma400_enable_interrupt(int_en, 2, &bma_device_handle);
    bma400_check_rslt("bma400_enable_interrupt", rslt);
}


int8_t bma400_setup_interrupts(bool step_counter, bool activity_monitor_interrupt, pin_id_t pin_id, bma400_data_change_callback_t bma400_data_change_cb)
{
    int8_t rslt = BMA400_OK;
    accelerometer_interrupt_pin = pin_id;
    bma400_data_change_callback = bma400_data_change_cb;
    sched_register_task(&process_bma400_interrupt);

    rslt = bma400_init_device();
    bma400_check_rslt("bma400_init_device", rslt);

    uint8_t power_mode = (step_counter||activity_monitor_interrupt)? BMA400_MODE_NORMAL:BMA400_MODE_SLEEP;

    rslt = bma400_set_power_mode(power_mode, &bma_device_handle);
    bma400_check_rslt("bma400_set_power_mode", rslt);

    if(step_counter)
        bma400_step_counter_interrupt_init();

    if(activity_monitor_interrupt)
       bma400_motion_monitoring();

    rslt = bma400_init_interrupt_pins();

    // power_mode = (step_counter||activity_monitor_interrupt)? BMA400_MODE_LOW_POWER:BMA400_MODE_SLEEP;
        power_mode = BMA400_MODE_SLEEP;
    rslt = bma400_set_power_mode(power_mode, &bma_device_handle);
    bma400_check_rslt("bma400_set_power_mode", rslt);

    return rslt;
}

static void process_bma400_interrupt()
{
    int8_t rslt = 0;
    uint16_t int_status;
    uint8_t act_int;
    uint32_t step_count = 0;
    rslt = bma400_get_interrupt_status(&int_status, &bma_device_handle);
    bma400_check_rslt("bma400_get_interrupt_status", rslt);
    bool interrupt_pin_state = hw_gpio_get_in(accelerometer_interrupt_pin);
    
    if(interrupt_pin_state && (int_status & BMA400_ASSERTED_WAKEUP_INT))
    {
        log_print_string("activity detected\n");
        current_motion_state = true;
    }
    else if(!interrupt_pin_state && !(int_status & BMA400_ASSERTED_WAKEUP_INT))
    {
        log_print_string("inactivity detected\n");
        current_motion_state = false;
    }
    else
    {
        current_motion_state =interrupt_pin_state;
        bma400_data_t bma400_data = { .current_motion_state = current_motion_state, .steps = 255, .current_activity_type = 0 };
        if(bma400_data_change_callback)
            bma400_data_change_callback(bma400_data);
        sched_post_task(&process_bma400_interrupt);
        return;
    }

    rslt = bma400_get_steps_counted(&step_count, &act_int, &bma_device_handle);
    bma400_check_rslt("bma400_get_steps_counted", rslt);

    bma400_data_t bma400_data = { .current_motion_state = current_motion_state, .steps = step_count, .current_activity_type = 0 };
    if(bma400_data_change_callback)
        bma400_data_change_callback(bma400_data);
}

static int8_t bma400_step_counter_interrupt_init()
{
    int8_t rslt = 0;
    struct bma400_sensor_conf accel_settin[2] = { { 0 } };
    struct bma400_int_enable int_en[2];


    accel_settin[0].type = BMA400_STEP_COUNTER_INT;
    accel_settin[1].type = BMA400_ACCEL;

    rslt = bma400_get_sensor_conf(accel_settin, 2, &bma_device_handle);
    bma400_check_rslt("bma400_get_sensor_conf", rslt);

    accel_settin[0].param.step_cnt.int_chan = BMA400_UNMAP_INT_PIN;

    accel_settin[1].param.accel.odr = BMA400_ODR_100HZ;
    accel_settin[1].param.accel.range = BMA400_RANGE_2G;
    accel_settin[1].param.accel.data_src = BMA400_DATA_SRC_ACCEL_FILT_1;

    /* Set the desired configurations to the sensor */
    rslt = bma400_set_sensor_conf(accel_settin, 2, &bma_device_handle);
    bma400_check_rslt("bma400_set_sensor_conf", rslt);


    int_en[0].type = BMA400_STEP_COUNTER_INT_EN;
    int_en[0].conf = BMA400_ENABLE;

    rslt = bma400_enable_interrupt(int_en, 1, &bma_device_handle);
    bma400_check_rslt("bma400_enable_interrupt", rslt);
}

static void bma400_interrupt_callback(void* arg)
{
   timer_post_task_delay(&process_bma400_interrupt, 1000);
}