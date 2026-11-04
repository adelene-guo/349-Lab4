#include <stdint.h>
#include "context_switch.h"
#include <arm.h>
#include <kernel.h>
#include <syscall_thread.h>

int init_thread_context(tcb_t *tcb, void *fn, void *vargp) {
  if (user_top - stack_words < __thread_u_stacks_low ||
      kernel_top - stack_words < __thread_k_stacks_low) {
    return -1; //no space for new thread
  }

  uint32_t *user_stack = user_top - USER_STACK_FRAME_WORDS;
  user_top -= stack_words; //decrement user stack pointer by stack size

  uint32_t *kernel_stack = kernel_top - KERNEL_STACK_FRAME_WORDS;
  kernel_top -= stack_words; //decrement kernel stack pointer by stack size

  for (int i = 0; i < USER_STACK_FRAME_WORDS; i++) user_stack[i] = 0;
  user_stack[USER_STACK_FRAME_R0]   = (uint32_t)vargp; //arguments
  user_stack[USER_STACK_FRAME_LR]   = (uint32_t)thread_kill | THUMB_BIT; //kill thread if it returns
  user_stack[USER_STACK_FRAME_PC]   = (uint32_t)fn; //function pointer
  user_stack[USER_STACK_FRAME_XPSR] = INITIAL_XPSR; //set thumb bit

  for (int i = 0; i < KERNEL_STACK_FRAME_WORDS; i++) kernel_stack[i] = 0;
  kernel_stack[KERNEL_STACK_FRAME_WORDS - 1] = EXC_RETURN_THREAD_PSP; //return to thread mode using PSP for new threads

  tcb->psp_base   = user_stack;
  tcb->msp_base   = kernel_stack;
  tcb->svc_active = false;
  return 0;
}
