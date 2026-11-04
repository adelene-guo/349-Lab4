#include <stdlib.h>
#include <string.h>

#include <gpio.h>

#include <systick.h>
#include <lcd_driver.h>
#include <keypad_driver.h>


#include <timer.h>
#include <nvic.h>

#include "arm.h"
#include "kernel.h"
#include "timer.h"
#include "i2c.h"
#include "lcd_driver.h"
#include "printk.h"
#include "uart_polling.h"
#include "uart.h"
#include "stdint.h"
#include "syscall.h"
#include "stdio.h"
#include "unistd.h"
#include "string.h"
#include "arm.h"
#include "servok.h"

/** @brief USARTDIV value */
#define USART_DIV 0x8B

/** @brief Documents kernel main. */
int kernel_main( void ) {
  init_349(); // DO NOT REMOVE THIS LINE
  uart_init(USART_DIV);
  systick_init(1000);
  
  //lcd_driver_init();
  //lcd_clear();
  //systick_delay(1000);

  //servo_init();
  enter_user_mode();

  return 0;
}
