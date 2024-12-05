/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include "sths34.h"
#include "sths34_STM32.h"

#include "log.h"
#include "errors.h"
#include "hwi2c.h"
#include "hwsystem.h"
#include "stm32_common_gpio.h"
#include "timer.h"


/* Extern variables ----------------------------------------------------------*/

/* Private functions ---------------------------------------------------------*/
/*
 *   WARNING:
 *   Functions declare in this section are defined at the end of this file
 *   and are strictly related to the hardware platform used.
 *
 */
static int32_t platform_write(void *handle, uint8_t reg, const uint8_t *bufp,
                              uint16_t len);
static int32_t platform_read(void *handle, uint8_t reg, uint8_t *bufp,
                             uint16_t len);
static void platform_delay(uint32_t ms);
static void shts34_delay_us(uint32_t period);
static void shts34_interrupt_callback(void* arg);
static void process_sths34_interrupt();

static GPIO_InitTypeDef input_config
    = { .Mode = GPIO_MODE_IT_RISING_FALLING, .Pull = GPIO_NOPULL, .Speed = GPIO_SPEED_FREQ_LOW };

static stmdev_ctx_t dev_ctx;
static int wakeup_thread = 0;
i2c_handle_t* shts34_i2c;
bool shts34_inited = false;
bool shts34_interrupt_inited = false;
static pin_id_t shts34_interrupt_pin;
sths34_data_change_callback_t sths34_data_change_cb;

void shts34_interface_init(i2c_handle_t* handler)
{
  /* Initialize mems driver interface */
  dev_ctx.write_reg = platform_write;
  dev_ctx.read_reg = platform_read;
  dev_ctx.mdelay = platform_delay;
  dev_ctx.handle = handler;
  shts34_i2c = handler;
}

static int32_t shts34_init_interrupt_pins()
{
    if(!shts34_interrupt_inited)
    {
        hw_gpio_configure_pin_stm(shts34_interrupt_pin, &input_config);
        hw_gpio_configure_interrupt(
            shts34_interrupt_pin, GPIO_RISING_EDGE | GPIO_FALLING_EDGE, &shts34_interrupt_callback, NULL);
        shts34_interrupt_inited = true;
    }

    return 0;
}

static int32_t shts34_init_device()
{
  int32_t ret = 0;
  uint8_t whoami;
  if(!shts34_inited)
  {
     /* Check device ID */
    ret = sths34pf80_device_id_get(&dev_ctx, &whoami);
    
    if (whoami != STHS34PF80_ID)
      log_print_error_string("SHT34 communication error");
  }
} 

void shts34_set_interrupt_enabled_state(bool active)
{
    if(active)
        hw_gpio_enable_interrupt(shts34_interrupt_pin);
    else
        hw_gpio_disable_interrupt(shts34_interrupt_pin);
}

static void process_sths34_interrupt()
{
  sths34pf80_func_status_t func_status;
  uint8_t motion;
  uint8_t presence;

  /* handle event in a "thread" alike code */
  wakeup_thread = 0;
  motion = 0;
  presence = 0;

  sths34pf80_func_status_get(&dev_ctx, &func_status);

  presence = func_status.pres_flag;

  if (presence) {
    log_print_string("Start of Presence\r\n");
  } else {
    log_print_string("End of Presence\r\n");
  }

  motion = func_status.mot_flag;

  if (motion) {
    log_print_string("Motion Detected!\r\n");
  }
  if(sths34_data_change_cb)
    sths34_data_change_cb(motion, presence);
}

static void shts34_interrupt_callback(void* arg)
{
  sched_post_task(&process_sths34_interrupt);
}

int32_t shts34_setup_presence_detection(pin_id_t interrupt_pin, bool presence_interrupt, bool motion_interrupt, sths34_data_change_callback_t sths34_data_change_callback)
{
  uint8_t whoami;
  int32_t ret;
  sths34pf80_lpf_bandwidth_t lpf_m, lpf_p, lpf_p_m, lpf_a_t;

  shts34_interrupt_pin = interrupt_pin;
  sched_register_task(&process_sths34_interrupt);
  sths34_data_change_cb = sths34_data_change_callback;

  shts34_init_device();
  shts34_init_interrupt_pins();

  /* Set averages (AVG_TAMB = 8, AVG_TMOS = 32) */
  sths34pf80_avg_tobject_num_set(&dev_ctx, STHS34PF80_AVG_TMOS_32);
  sths34pf80_avg_tambient_num_set(&dev_ctx, STHS34PF80_AVG_T_8);

  /* Set BDU */
  sths34pf80_block_data_update_set(&dev_ctx, 1);

  sths34pf80_presence_threshold_set(&dev_ctx, 300);
  sths34pf80_presence_hysteresis_set(&dev_ctx, 10);
  sths34pf80_motion_threshold_set(&dev_ctx, 300);
  sths34pf80_motion_hysteresis_set(&dev_ctx, 30);

  sths34pf80_algo_reset(&dev_ctx);

  uint8_t interrupt_mode = STHS34PF80_INT_ALL;
  interrupt_mode = !presence_interrupt ? STHS34PF80_INT_MOTION: interrupt_mode;
  interrupt_mode = !motion_interrupt ? STHS34PF80_INT_PRESENCE : interrupt_mode;

  /* Set interrupt */
  sths34pf80_int_or_set(&dev_ctx, interrupt_mode);
  sths34pf80_route_int_set(&dev_ctx, STHS34PF80_INT_OR);

  // uint8_t ODR_mode = presence_interrupt || motion_interrupt ? STHS34PF80_ODR_AT_2Hz : STHS34PF80_ODR_OFF;
  uint8_t ODR_mode = STHS34PF80_ODR_OFF;

  /* Set ODR */
  sths34pf80_odr_set(&dev_ctx, ODR_mode);
}

/*
 * @brief  Write generic device register (platform dependent)
 *
 * @param  handle    customizable argument. In this examples is used in
 *                   order to select the correct sensor bus handler.
 * @param  reg       register to write
 * @param  bufp      pointer to data to write in register reg
 * @param  len       number of consecutive register to write
 *
 */
static int32_t platform_write(void *handle, uint8_t reg, const uint8_t *bufp,
                              uint16_t len)
{
  return !i2c_write_memory(shts34_i2c, STHS34PF80_I2C_ADD, reg, 8, (uint8_t*) bufp, len);
}

/*
 * @brief  Read generic device register (platform dependent)
 *
 * @param  handle    customizable argument. In this examples is used in
 *                   order to select the correct sensor bus handler.
 * @param  reg       register to read
 * @param  bufp      pointer to buffer that store the data read
 * @param  len       number of consecutive register to read
 *
 */
static int32_t platform_read(void *handle, uint8_t reg, uint8_t *bufp,
                             uint16_t len)
{
  return !i2c_read_memory(shts34_i2c, STHS34PF80_I2C_ADD, reg, 8, (uint8_t*) bufp, len);
}


/*
 * @brief  platform specific delay (platform dependent)
 *
 * @param  ms        delay in ms
 *
 */
static void platform_delay(uint32_t ms)
{
  shts34_delay_us(ms*1000);
}


/*!
 * Delay function
 */
static void shts34_delay_us(uint32_t period)
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