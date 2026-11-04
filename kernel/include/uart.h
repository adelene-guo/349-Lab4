/**
 * @file
 *
 * @brief
 *
 * @date
 *
 * @author
 */

#ifndef _UART_H_
#define _UART_H_

/** @brief buffer capacity (power of 2) */
#define UART_BUF_SIZE 64

/** @brief Documents buf t. */
typedef struct {
  char data[UART_BUF_SIZE];          /**< Circular buffer storage. */
  volatile uint32_t head;            /**< Index of the next byte to read. */
  volatile uint32_t tail;            /**< Index of the next free slot to write. */
} buf_t;

/** @brief Documents uart tx buf. */
extern buf_t uart_tx_buf;
/** @brief Documents uart rx buf. */
extern buf_t uart_rx_buf;

/** @brief Documents uart init. */
void uart_init(int baud);

/** @brief Documents uart put byte. */
int uart_put_byte(char c);

/** @brief Documents uart get byte. */
int uart_get_byte(char *c);

/** @brief Documents uart write. */
int uart_write( int file, char *ptr, int len );

/** @brief Documents uart read. */
int uart_read(int file, char *ptr, int len );

/** @brief Documents uart flush. */
void uart_flush();

#endif /* _UART_H_ */