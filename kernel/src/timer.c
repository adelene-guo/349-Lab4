/**
 * @file timer.c
 *
 * @brief This file contains the implementation of the timer driver for the STM32F4 microcontroller.
 *
 * @date 11/3/2026
 *
 * @author Alexis and Adie
 */

#include <unistd.h>
#include <timer.h>
#include <rcc.h>
#include <nvic.h>
#include <gpio.h>
#include <systick.h>

/** @brief Documents UNUSED. */
#define UNUSED __attribute__((unused))

/** @brief enable bit for update generation */
#define TIM_EGR_UG (1 << 0)

/** @brief status register update interrupt flag */
#define TIM_SR_UIF (1 << 0)

/** @brief enable bit for update interrupt */
#define TIM_DIER_UIE (1 << 0)

/** @brief enable bit for counter */
#define TIM_CR1_CEN (1 << 0)

/** @brief tim2_5 */
struct tim2_5 {
  volatile uint32_t cr1; /**< 00 Control Register 1 */
  volatile uint32_t cr2; /**< 04 Control Register 2 */
  volatile uint32_t smcr; /**< 08 Slave Mode Control */
  volatile uint32_t dier; /**< 0C DMA/Interrupt Enable */
  volatile uint32_t sr; /**< 10 Status Register */
  volatile uint32_t egr; /**< 14 Event Generation */
  volatile uint32_t ccmr[2]; /**< 18-1C Capture/Compare Mode */
  volatile uint32_t ccer; /**< 20 Capture/Compare Enable */
  volatile uint32_t cnt; /**< 24 Counter Register */
  volatile uint32_t psc; /**< 28 Prescaler Register */
  volatile uint32_t arr; /**< 2C Auto-Reload Register */
  volatile uint32_t reserved_1; /**< 30 */
  volatile uint32_t ccr[4]; /**< 34-40 Capture/Compare */
  volatile uint32_t reserved_2; /**< 44 */
  volatile uint32_t dcr; /**< 48 DMA Control Register */
  volatile uint32_t dmar; /**< 4C DMA address for full transfer Register */
  volatile uint32_t or; /**< 50 Option Register */
};

/** @brief Documents timer base[]. */
struct tim2_5* const timer_base[] = {(void *)0x0,   // N/A - Don't fill out
                                     (void *)0x0,   // N/A - Don't fill out
                                     (void *)0x40000000,    // address for TIMER 2
                                     (void *)0x40000400,    // address for TIMER 3
                                     (void *)0x40000800,    // address for TIMER 4
                                     (void *)0x40000C00};   // address for TIMER 5


/** @brief Documents timer irqn[]. */
int timer_irqn[] = {0, 0, 28, 29, 30, 50};


/** @brief Documents timer init. */
void timer_init(UNUSED int timer, UNUSED uint32_t prescalar, UNUSED uint32_t period) {
  if (timer<2 || timer>5) return;

  struct rcc_reg_map *rcc = RCC_BASE;
  rcc->apb1_enr |= 1 << (timer-2); // enable RCC for appropriate timer

  struct tim2_5 *tim = timer_base[timer];
  tim->psc = prescalar;
  tim->arr = period;
  tim->egr |= TIM_EGR_UG; // update event to load the active prescaler register
  tim->sr &= ~TIM_SR_UIF; // clear update interrupt flag set by update event
  tim->dier |= TIM_DIER_UIE; // enable update interrupt
  nvic_irq(timer_irqn[timer], IRQ_ENABLE); // enable the interrupt

  tim->cr1 |= TIM_CR1_CEN; // enable counter
}

/** @brief Documents timer disable. */
void timer_disable(UNUSED int timer) {
  if (timer<2 || timer>5) return;

  struct tim2_5 *tim = timer_base[timer];
  tim->dier &= ~TIM_DIER_UIE; // disable update interrupt
  tim->cr1 &= ~TIM_CR1_CEN; // disable counter

  struct rcc_reg_map *rcc = RCC_BASE;
  rcc->apb1_enr &= ~(1 << (timer-2)); // disable RCC for appropriate timer
}

/** @brief Documents timer clear interrupt bit. */
void timer_clear_interrupt_bit(UNUSED int timer) {
  if (timer<2 || timer>5) return;

  struct tim2_5 *tim = timer_base[timer];
  tim->sr &= ~TIM_SR_UIF;
}

// Interrupt handler for when testing timer with LED in section 10
// int led_on;
// void TIM2_IRQHandler(void) {
//   timer_clear_interrupt_bit(2);
//   gpio_port gpio_portGreen = GPIO_A;
//   unsigned int numGreen    = 8;
//   led_on = !led_on;
//   if(led_on) gpio_set(gpio_portGreen, numGreen);
//   else gpio_clr(gpio_portGreen, numGreen);
// }

/** @brief Documents TIM3 IRQHandler. */
void TIM3_IRQHandler(void) {
  timer_clear_interrupt_bit(3);
}

/** @brief Documents TIM4 IRQHandler. */
void TIM4_IRQHandler(void) {
  timer_clear_interrupt_bit(4);
}

/** @brief Documents TIM5 IRQHandler. */
void TIM5_IRQHandler(void) {
  timer_clear_interrupt_bit(5);
}