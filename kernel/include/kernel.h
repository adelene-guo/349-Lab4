/**
 * @file   kernel.h
 *
 * @brief  Kernel entry point header file.
 *
 * @date   August 28 2015
 * @author Ronit Banerjee <ronitb@andrew.cmu.edu>
 */

#ifndef _KERNEL_H_
#define _KERNEL_H_

#include <stdint.h>

/**
 * @brief      Symbol for assembly method, for linker resolution.
 */
extern void enter_user_mode( void );

/**
 * @brief      Symbol for getting the current psp, for linker resolution.
 */
extern uint32_t *get_psp( void );
void default_idle(void);


/**
 * @brief      Symbol for setting the current psp, for linker resolution.
 */
extern void set_psp( uint32_t *psp );


#endif /* _KERNEL_H_ */
