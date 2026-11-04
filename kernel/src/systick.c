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
#include <arm.h>
#include <syscall_thread.h>

extern volatile uint32_t g_sys_tick_count;

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

/** @brief Global counter of ticks since boot*/
volatile uint32_t systick_ticks = 0;

/** @brief Documents systick init. */
void systick_init(uint32_t frequency) {
    struct systick_reg_map *systick = SYSTICK_BASE;
    if (frequency == 0 || frequency > BASE_FREQ) return;
    systick->STK_CTRL = 0;
    systick->STK_LOAD = BASE_FREQ / frequency - 1U;
    systick->STK_VAL = 0;
    systick->STK_CTRL = TICKINT | CLKSOURCE | ENABLE;
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
    systick_ticks++;
    ++g_sys_tick_count;
    if (g_scheduler_running) pend_pendsv();
}
