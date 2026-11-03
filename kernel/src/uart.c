/**
 * @file uart.c
 *
 * @brief This file contains the implementation of the UART driver for the STM32F4 microcontroller.
 * It provides functions to initialize the UART and communicate with other devices.     
 *
 * @date 11/3/2026  
 *
 * @author Adie and Alexis
 */

#include <stdint.h>
#include <unistd.h>
#include <rcc.h>
#include <uart.h>
#include <uart_polling.h>
#include <nvic.h>
#include <gpio.h>


/** @brief Documents UNUSED. */
#define UNUSED __attribute__((unused))

/** @brief The UART register map. */
struct uart_reg_map {
    volatile uint32_t SR;   /**< Status Register */
    volatile uint32_t DR;   /**<  Data Register */
    volatile uint32_t BRR;  /**<  Baud Rate Register */
    volatile uint32_t CR1;  /**<  Control Register 1 */
    volatile uint32_t CR2;  /**<  Control Register 2 */
    volatile uint32_t CR3;  /**<  Control Register 3 */
    volatile uint32_t GTPR; /**<  Guard Time and Prescaler Register */
};

/** @brief Base address for UART2 */
#define UART2_BASE  (struct uart_reg_map *) 0x40004400

/** @brief IRQ number for UART2 */
#define UART2_IRQ  38

/** @brief Enable bit for UART Config register */
#define UART_EN (1 << 13)

/** @brief Enable bit for UART Clock */
#define RCC_EN (1 << 17)

/** @brief Transmitter enable */
#define TRANSMITTER_EN (1 << 2)

/** @brief Receiver enable */
#define RECEIVER_EN (1 << 3)

/** @brief Transmit data register empty bit of CR */
#define UART_SR_TXE (1 << 7)

/** @brief Read data register not empty bit of CR */
#define UART_SR_RXNE (1 << 5)

/** @brief Transmission complete */
#define UART_SR_TC (1 << 6)

/** @brief RX interrupe enable bit */
#define UART_CR1_RXNEIE (1 << 5)

/** @brief TX interrupt enable bit */
#define UART_CR1_TXEIE (1 << 7)

/** @brief mask for buffer size instead of modding */
#define UART_BUF_MASK (UART_BUF_SIZE-1)


/** @brief Documents uart tx buf. */
buf_t uart_tx_buf;
/** @brief Documents uart rx buf. */
buf_t uart_rx_buf;

/** @brief Documents empty. */
int empty(buf_t *b) {
  return b->head == b->tail;
}

/** @brief Documents full. */
int full(buf_t *b) {
  return b->head - b->tail == UART_BUF_SIZE;
}

/** @brief Documents push. */
void push(buf_t *b, char c) {
  uint32_t index = b->head++ & UART_BUF_MASK; // inrement head and "mod"
  b->data[index] = c;
}

/** @brief Documents pop. */
char pop(buf_t *b) {
  uint32_t index = b->tail++ & UART_BUF_MASK; // increment tail and "mod"
  return b->data[index];
}


/** @brief Documents uart init. */
void uart_init(UNUSED int baud){
    struct rcc_reg_map *rcc = RCC_BASE;
    rcc->apb1_enr |= RCC_EN;
    
    struct uart_reg_map *uart = UART2_BASE;
    uart->CR1 |= UART_EN;
    uart->CR1 |= TRANSMITTER_EN;
    uart->CR1 |= RECEIVER_EN;
    uart->BRR |= baud;

    uart->CR1 |= UART_CR1_RXNEIE;
    // uart->CR1 &= ~UART_CR1_TXEIE; should be set to 0

    gpio_port gpio_portTX = GPIO_A;
    unsigned int numTX    = 2;
    unsigned int modeTX   = MODE_ALT;
    unsigned int otypeTX  = OUTPUT_PUSH_PULL;
    unsigned int speedTX  = OUTPUT_SPEED_LOW;
    unsigned int pupdTX   = PUPD_NONE;
    unsigned int altTX    = ALT7;  
    gpio_init(gpio_portTX, numTX, modeTX, otypeTX, speedTX, pupdTX, altTX);

    gpio_port gpio_portRX = GPIO_A;
    unsigned int numRX    = 3;
    unsigned int modeRX   = MODE_ALT;
    unsigned int otypeRX  = OUTPUT_OPEN_DRAIN;
    unsigned int speedRX  = OUTPUT_SPEED_LOW;
    unsigned int pupdRX   = PUPD_NONE;
    unsigned int altRX    = ALT7;
    gpio_init(gpio_portRX, numRX, modeRX, otypeRX, speedRX, pupdRX, altRX);

    nvic_irq(UART2_IRQ, IRQ_ENABLE);
    return;
}

/** @brief Documents uart put byte. */
int uart_put_byte(UNUSED char c){
  struct uart_reg_map *uart = UART2_BASE;
  int success = -1;
  nvic_irq(UART2_IRQ, IRQ_DISABLE);
  if (!full(&uart_tx_buf)) {
    if (empty(&uart_tx_buf)) {
      uart->CR1 |= UART_CR1_TXEIE;
    }
    push(&uart_tx_buf, c);
    success = 0;
  }
  nvic_irq(UART2_IRQ, IRQ_ENABLE);
  return success;
}

/** @brief Documents uart get byte. */
int uart_get_byte(UNUSED char *c){
  struct uart_reg_map *uart = UART2_BASE;
  int success = -1;
  nvic_irq(UART2_IRQ, IRQ_DISABLE);
  if (!empty(&uart_rx_buf)) {
    if (full(&uart_rx_buf)) {
      uart->CR1 |= UART_CR1_RXNEIE;
    }
    *c = pop(&uart_rx_buf);
    success = 0;
  }
  nvic_irq(UART2_IRQ, IRQ_ENABLE);
  return success;
}

/** @brief Documents USART2 TX IRQHandler. */
void USART2_TX_IRQHandler() {
  struct uart_reg_map *uart = UART2_BASE;
  uart->DR = pop(&uart_tx_buf);
  if (empty(&uart_tx_buf)) {
    uart->CR1 &= ~UART_CR1_TXEIE; // set TXEIE to 0
  }
}

/** @brief Documents USART2 RX IRQHandler. */
void USART2_RX_IRQHandler() {
  struct uart_reg_map *uart = UART2_BASE;
  push(&uart_rx_buf, uart->DR);
  if (full(&uart_rx_buf)) {
    uart->CR1 &= ~UART_CR1_RXNEIE; // set RXNEIE to 0
  }
}

/** @brief Documents USART2 IRQHandler. */
void USART2_IRQHandler(){
  struct uart_reg_map *uart = UART2_BASE;
  uint32_t rxne = uart->SR & UART_SR_RXNE;
  uint32_t rxneie = uart->CR1 & UART_CR1_RXNEIE;
  if (rxne && rxneie) {
    USART2_RX_IRQHandler();
  }

  uint32_t txe = uart->SR & UART_SR_TXE;
  uint32_t txeie = uart->CR1 & UART_CR1_TXEIE;
  if (txe && txeie) {
    USART2_TX_IRQHandler();
  }
  nvic_clear_pending(UART2_IRQ);
}

/** @brief Documents uart flush. */
void uart_flush(){
  struct uart_reg_map *uart = UART2_BASE;
  nvic_irq(UART2_IRQ, IRQ_DISABLE);
  while (!empty(&uart_tx_buf)) {
    while (!(uart->SR & UART_SR_TXE)) {}
    USART2_TX_IRQHandler(); 
  }
  while (!(uart->SR & UART_SR_TC)){};
}