/**
 * @file svc_handler.c
 *
 * @brief This handler is called when a SVC instruction is executed.
 * It extracts the SVC number and arguments from the stack, 
 * calls the appropriate system call handler, and stores the return value back on the stack. 
 *
 * @date 11/3/2026
 *
 * @author Adie and Alexis
 */

#include <stdint.h>
#include <debug.h>
#include <syscall.h>
#include <servok.h>
#include <svc_num.h>


/** @brief Documents UNUSED. */
#define UNUSED __attribute__((unused))

/** @brief Documents svc c handler. */
void svc_c_handler(uint32_t *psp) {
  uint8_t *svc_instr = (uint8_t *)psp[6] - 2;   // address of the svc instruction
  uint8_t svc_num = svc_instr[0];               // low byte of the halfword = immediate
  int return_val;
  switch (svc_num) {
    case SVC_WRITE:
      return_val = sys_write((int)psp[0], (char *)psp[1], (int)psp[2]);
      break;
    case SVC_READ:
      return_val = sys_read((int)psp[0], (char *)psp[1], (int)psp[2]);
      break;
    case SVC_SBRK:
      return_val = (int)sys_sbrk((int)psp[0]);
      break;
    case SVC_EXIT:
      sys_exit((int)psp[0]);
      return_val = 0;
      break;
    case SVC_SERVO_ENABLE:
      return_val = sys_servo_enable((uint8_t)psp[0], (uint8_t)psp[1]);
      break;
    case SVC_SERVO_SET:
      return_val = sys_servo_set((uint8_t)psp[0], (uint8_t)psp[1]);
      break;
    default:
      return_val = -1;
      break;
  }

  psp[0] = (uint32_t)return_val;
}