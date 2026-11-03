/**
 * @file systick.c
 *
 * @brief
 * This file contains the implementation for the SysTick interrupt controller
 * @date 10/18/2026
 *
 * @author Adie and Alexis
 */

#include <unistd.h>
#include <systick.h>

/** @brief Documents UNUSED. */
#define UNUSED __attribute__((unused))

/** @brief The SysTick register map. */
struct systick_reg_map {
    volatile uint32_t STK_CTRL;   /**<  Control register */
    volatile uint32_t STK_LOAD;   /**<  Load register */
    volatile uint32_t STK_VAL;   /**<  Value register */
    volatile uint32_t STK_CALIB;   /**<  Calibration register */
};

/** @brief Base address for SysTick */
#define SYSTICK_BASE  (struct systick_reg_map *) 0xE000E010

/** @brief CLKSOURCE bit */
#define CLKSOURCE (1 << 2)

/** @brief TICKINT bit */
#define TICKINT (1 << 1)

/** @brief ENABLE bit */
#define ENABLE 1

/** @brief COUNTFLAG bit */
#define COUNTFLAG (1 << 16)

/** @brief RELOAD value for 1 ms tick (RELOAD = 1ms/T_freq - 1)*/
#define RELOAD 15999

/** @brief Global counter of ticks since boot*/
volatile uint32_t systick_ticks = 0;

/** @brief Documents systick init. */
void systick_init() {
    struct systick_reg_map *systick = SYSTICK_BASE;

    systick->STK_LOAD = RELOAD; //set reload value to be loaded

    systick->STK_CTRL |= TICKINT;
    systick->STK_CTRL |= CLKSOURCE;
    systick->STK_CTRL |= ENABLE; //load reload val and start timer
}

/** @brief Documents systick delay. */
void systick_delay(uint32_t ticks) {
    uint32_t start = systick_get_ticks();
    
    while (systick_get_ticks() - start < ticks); //wait for ticks to pass
}

/** @brief Documents systick get ticks. */
uint32_t systick_get_ticks() {
    return systick_ticks; //return global tick counter
}

/** @brief Documents systick c handler. */
void systick_c_handler() {
    systick_ticks++; //increment global tick counter
}