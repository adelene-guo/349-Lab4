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

.thumb_func
.global get_psp
get_psp:
  mrs r0, psp
  bx lr

.thumb_func
.global set_psp
set_psp:
  msr psp, r0
  bx lr

.thumb_func
.global default_idle
default_idle:
  wfi
  b default_idle
