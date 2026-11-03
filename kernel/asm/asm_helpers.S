.cpu cortex-m4
.syntax unified
.thumb

.section .text

.thumb_func
.global enter_user_mode
enter_user_mode:
  push {r0, lr}
  ldr r0, =__psp_stack_top
  msr psp, r0
  blx _crt0
  pop {r0, lr}
  bx lr