#ifndef CONTEXT_SWITCH_H
#define CONTEXT_SWITCH_H

#include <stdint.h>
#include <syscall_thread.h>

#define EXC_RETURN_THREAD_PSP  0xFFFFFFFD
#define INITIAL_XPSR           0x01000000
#define USER_STACK_FRAME_WORDS         8
#define KERNEL_STACK_FRAME_WORDS         10   //r4-r11, r12 padding, and lr

#define USER_STACK_FRAME_R0   0
#define USER_STACK_FRAME_LR   5
#define USER_STACK_FRAME_PC   6
#define USER_STACK_FRAME_XPSR 7

#define THUMB_BIT 1

/** @brief Documents user stack top and bottom. */
extern uint32_t __thread_u_stacks_top[];
extern uint32_t __thread_u_stacks_low[];

/** @brief Documents kernel stack top and bottom. */
extern uint32_t __thread_k_stacks_top[];
extern uint32_t __thread_k_stacks_low[];

/** @brief Documents thread kill. */
extern void thread_kill(void);

/** @brief Size in words of each thread stack, set by thread_init. */
extern uint32_t stack_words;

/** @brief Initialize thread context.
 *  @param tcb Pointer to thread control block.
 *  @param fn Function pointer for thread entry point.
 *  @param vargp Argument pointer for thread.
 *  @return 0 on success, -1 on failure.
 */
int init_thread_context(tcb_t *tcb, void *fn, void *vargp);

#endif
