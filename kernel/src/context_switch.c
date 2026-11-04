#include <stdint.h>
#include "context_switch.h"
#include <arm.h>
#include <kernel.h>
#include <syscall_thread.h>

extern tcb_t g_tcbs[THREAD_TCB_COUNT];

int init_thread_context(tcb_t *tcb, void *fn, void *vargp) {
  if (stack_words == 0) return -1;

  uint32_t user_slots = (uint32_t)(__thread_u_stacks_top - __thread_u_stacks_low) / stack_words;
  uint32_t kernel_slots = (uint32_t)(__thread_k_stacks_top - __thread_k_stacks_low) / stack_words;
  uint32_t available_slots = user_slots < kernel_slots ? user_slots : kernel_slots; // minimum of user and kernel stack slots
  if (available_slots > THREAD_STACK_SLOTS) available_slots = THREAD_STACK_SLOTS; // limit to the maximum number of stack slots

  uint32_t slot = tcb->stack_slot; // existing stack slot
  if (slot >= available_slots) { // no existing stack slot
    for (slot = 0; slot < available_slots; ++slot) {
      bool slot_used = false;
      for (uint32_t i = 0; i < THREAD_TCB_COUNT; ++i) { // check if any other TCB is using this stack slot
        if (&g_tcbs[i] != tcb && g_tcbs[i].stack_slot == slot) {
          slot_used = true;
          break;
        }
      }
      if (!slot_used) break; // found an available stack slot
    }
    if (slot == available_slots) return -1; // no available stack slots
  }

  tcb->stack_slot = (uint8_t)slot; // updating tcb and stack pointers
  uint32_t *user_stack_top = __thread_u_stacks_top - slot * stack_words;
  uint32_t *kernel_stack_top = __thread_k_stacks_top - slot * stack_words;
  uint32_t *user_stack = user_stack_top - USER_STACK_FRAME_WORDS;
  uint32_t *kernel_stack = kernel_stack_top - KERNEL_STACK_FRAME_WORDS;

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
