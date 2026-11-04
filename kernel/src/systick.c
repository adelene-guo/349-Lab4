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
    if (frequency == 0) { // disable SysTick
        systick->STK_CTRL = 0;
        return;
    }
    if (frequency > BASE_FREQ) return; // frequency too high
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
    tcb_t *curr_tcb = g_current_tcb;
    if (curr_tcb->thread_state==THREAD_RUNNING && //if there is a running thread
        curr_tcb!=g_idle_tcb && curr_tcb!=g_default_tcb) { // non-idle, non-default
        curr_tcb->curr_tick_count++;
        curr_tcb->total_tick_count++;
        if (curr_tcb->curr_tick_count >= curr_tcb->C) { // over computation lim
            curr_tcb->thread_state=THREAD_WAITING;
        }
    }

    for (uint32_t i=0; i<MAX_USER_THREADS; i++) {
        tcb_t *tcb = &g_tcbs[i];
        if (tcb->thread_state==THREAD_WAITING && g_sys_tick_count>=tcb->next_wake_up){
            tcb->thread_state = THREAD_RUNNABLE;
            tcb->curr_tick_count = 0;
            tcb->next_wake_up += tcb->T;
        }
    }
    pend_pendsv();
}

void systick_stop() {
    struct systick_reg_map *systick = SYSTICK_BASE;
    systick->STK_CTRL &= ~ENABLE; // disable the enable bit
    return;
}