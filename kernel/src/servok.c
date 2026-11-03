/**
 * @file servok.c
 *
 * @brief This file contains the implementation of the servo driver for the STM32F4 microcontroller. It provides functions to initialize the servo and control its position.   
 *
 * @date October 30, 2026       
 *
 * @author Alexis and Adie    
 */

#include <unistd.h>
#include <stdint.h>
#include <gpio.h>
#include <timer.h>

/** @brief Documents UNUSED. */
#define UNUSED __attribute__((unused))

// D2
/** @brief Documents SERVO1 PORT. */
#define SERVO1_PORT GPIO_A // pwm1
/** @brief Documents SERVO1 PIN. */
#define SERVO1_PIN 10

// D3
/** @brief Documents SERVO2 PORT. */
#define SERVO2_PORT GPIO_B // pwm2
/** @brief Documents SERVO2 PIN. */
#define SERVO2_PIN 10

/** @brief Documents SERVO TIMER. */
#define SERVO_TIMER 2
/** @brief Documents SERVO PRESCALAR. */
#define SERVO_PRESCALAR 16 // 16MHz/16 = 1MHz --> 1us time units
/** @brief Documents SERVO PERIOD. */
#define SERVO_PERIOD 20 // 20us between interrupts
/** @brief Documents SERVO MAX ANGLE. */
#define SERVO_MAX_ANGLE 180
/** @brief Documents SERVO FRAME. */
#define SERVO_FRAME 20000 // 20ms
#define SERVO_MIN_US 600   /**< Pulse width at 0 degrees. */
#define SERVO_US_PER_DEGREE (10)    /**< (2400 us - 600 us) / 180 degrees. */

/** @brief Documents PWM CH1. */
#define PWM_CH1 1
/** @brief Documents PWM CH2. */
#define PWM_CH2 2

/** @brief Documents servo port[2]. */
gpio_port servo_port[2] = {SERVO1_PORT, SERVO2_PORT};
/** @brief Documents servo pin[2]. */
uint32_t servo_pin[2] = {SERVO1_PIN, SERVO2_PIN};

static volatile uint32_t servo_enabled[2];
static volatile uint32_t servo_high[2];
static volatile uint32_t servo_active_interrupts[2]; // # interrupts that servo needs to enabled
static volatile uint32_t servo_interrupt_counter;


/** @brief Documents servo init. */
void servo_init(void) {
  gpio_init(servo_port[0], servo_pin[0], MODE_GP_OUTPUT, OUTPUT_PUSH_PULL, 
    OUTPUT_SPEED_LOW, PUPD_NONE, ALT0);
  gpio_clr(servo_port[0], servo_pin[0]);  

  gpio_init(servo_port[1], servo_pin[1], MODE_GP_OUTPUT, OUTPUT_PUSH_PULL, 
    OUTPUT_SPEED_LOW, PUPD_NONE, ALT0);
  gpio_clr(servo_port[1], servo_pin[1]);  

  timer_init(SERVO_TIMER, SERVO_PRESCALAR-1, SERVO_PERIOD-1);
}

/** @brief Documents sys servo enable. */
int sys_servo_enable(UNUSED uint8_t channel, UNUSED uint8_t enabled){
  if (channel!=PWM_CH1 && channel!=PWM_CH2) return -1;

  servo_enabled[channel-1] = enabled ? 1 : 0;
  return 0;
}

/** @brief Documents sys servo set. */
int sys_servo_set(UNUSED uint8_t channel, UNUSED uint8_t angle){
  if ((channel!=PWM_CH1 && channel!=PWM_CH2) || angle>SERVO_MAX_ANGLE) 
    return -1;
  servo_active_interrupts[channel-1] = 
    (angle*SERVO_US_PER_DEGREE + SERVO_MIN_US) / SERVO_PERIOD;
  return 0;
}

/** @brief Documents TIM2 IRQHandler. */
void TIM2_IRQHandler(void) {
  timer_clear_interrupt_bit(SERVO_TIMER);
  uint32_t high0 = servo_enabled[0] && servo_interrupt_counter<servo_active_interrupts[0];
  if (high0 != servo_high[0]) {
    servo_high[0] = high0;
    if (high0) {
      gpio_set(servo_port[0], servo_pin[0]);
    } else {
      gpio_clr(servo_port[0], servo_pin[0]);
    }
  }

  uint32_t high1 = servo_enabled[1] && servo_interrupt_counter<servo_active_interrupts[1];
  if (high1 != servo_high[1]) {
    servo_high[1] = high1;
    if (high1) {
      gpio_set(servo_port[1], servo_pin[1]);
    } else {
      gpio_clr(servo_port[1], servo_pin[1]);
    }
  }

  servo_interrupt_counter++;
  if (servo_interrupt_counter == (SERVO_FRAME / SERVO_PERIOD)) {
    servo_interrupt_counter = 0;
  }
}