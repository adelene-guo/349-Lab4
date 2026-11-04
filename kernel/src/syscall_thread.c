#include <stdint.h>
#include <stdbool.h>
#include "syscall_thread.h"
#include "syscall_mutex.h"
#include <arm.h>
#include <kernel.h>
#include <context_switch.h>
#include <systick.h>
#include <mpu.h>

#define NUM_TCBS 16
#define MAX_USER_THREADS 14
#define IDLE_THREAD_INDEX 14
#define DEFAULT_THREAD_INDEX 15
#define MIN_STACK_WORDS 256

float ub_table[] = {
  0.000, 1.000, .8284, .7798, .7568,
  .7435, .7348, .7286, .7241, .7205,
  .7177, .7155, .7136, .7119, .7106,
  .7094, .7083, .7075, .7066, .7059,
  .7052, .7047, .7042, .7037, .7033,
  .7028, .7025, .7021, .7018, .7015,
  .7012, .7009
};

tcb_t g_tcbs[NUM_TCBS];
tcb_t *g_current_tcb;
tcb_t *g_idle_tcb;
tcb_t *g_default_tcb;
uint32_t g_nonidle_threads_count;
uint32_t g_max_threads;
uint32_t g_max_mutexes;
bool g_threads_initialized;
bool g_scheduler_running;
uint32_t g_scheduler_freq;
volatile uint32_t g_sys_tick_count;
uint32_t *user_top;
uint32_t *kernel_top;
uint32_t stack_words;

extern void default_idle(void);

static bool passes_ub_test(uint32_t new_C, uint32_t new_T) {
  if (new_T == 0) return false;
  float utilization = (float)new_C / (float)new_T;
  uint32_t count = 1;
  for (uint32_t i = 0; i < MAX_USER_THREADS; ++i) {
    if (g_tcbs[i].thread_state == THREAD_UNUSED) continue;
    if (g_tcbs[i].T == 0) return false;
    utilization += (float)g_tcbs[i].C / (float)g_tcbs[i].T;
    ++count;
  }
  return count < sizeof(ub_table) / sizeof(ub_table[0]) && utilization <= ub_table[count];
}

tcb_t *pick_next_thread(void) {
  uint32_t start = (uint32_t)(g_current_tcb - g_tcbs);
  for (uint32_t offset = 1; offset <= NUM_TCBS; ++offset) {
    tcb_t *candidate = &g_tcbs[(start + offset) % NUM_TCBS];
    if (candidate->thread_state == THREAD_RUNNABLE) return candidate;
  }
  return g_idle_tcb;
}

void *pendsv_c_handler(void *context_ptr) {
  tcb_t *prev = g_current_tcb; // save the current thread's context
  prev->msp_base = context_ptr;
  prev->psp_base = get_psp();
  prev->svc_active = get_svc_status() != 0;
  if (prev->thread_state == THREAD_RUNNING && prev != g_default_tcb)
    prev->thread_state = THREAD_RUNNABLE;

  tcb_t *next = pick_next_thread(); // pick the next thread to run
  next->thread_state = THREAD_RUNNING;
  g_current_tcb = next;
  set_psp(next->psp_base);
  set_svc_status(next->svc_active);
  return next->msp_base;
}

int sys_thread_init(uint32_t max_threads, uint32_t stack_size, void *idle_fn,
                    uint32_t max_mutexes) {
  if (max_threads == 0 || max_threads > MAX_USER_THREADS) return -1; // invalid number of threads
  uint32_t words = stack_size < MIN_STACK_WORDS ? MIN_STACK_WORDS : stack_size; // ensure minimum stack size
  words = 1U << mm_log2ceil_size(words);
  uint32_t user_region_words = (uint32_t)(__thread_u_stacks_top - __thread_u_stacks_low);
  uint32_t kernel_region_words = (uint32_t)(__thread_k_stacks_top - __thread_k_stacks_low);
  if ((max_threads + 1U) * words > user_region_words ||
      (max_threads + 1U) * words > kernel_region_words) return -1; // insufficient memory

  for (uint32_t i = 0; i < NUM_TCBS; ++i) g_tcbs[i].thread_state = THREAD_UNUSED;
  g_max_threads = max_threads;
  g_max_mutexes = max_mutexes;
  stack_words = words;
  user_top = __thread_u_stacks_top;
  kernel_top = __thread_k_stacks_top;
  g_nonidle_threads_count = 0;
  g_idle_tcb = &g_tcbs[IDLE_THREAD_INDEX];
  g_default_tcb = &g_tcbs[DEFAULT_THREAD_INDEX];
  g_current_tcb = g_default_tcb;

  void *idle = idle_fn != NULL ? idle_fn : (void *)default_idle; // use default idle function if none provided
  if (init_thread_context(g_idle_tcb, idle, NULL) != 0) return -1; // failed to initialize idle thread context
  g_idle_tcb->static_priority = IDLE_THREAD_INDEX;
  g_idle_tcb->dynamic_priority = IDLE_THREAD_INDEX;
  g_idle_tcb->thread_state = THREAD_RUNNABLE;
  g_threads_initialized = true;
  return 0;
}

int sys_thread_create(void *fn, uint32_t prio, uint32_t C, uint32_t T, void *vargp) {
  if (!g_threads_initialized || prio >= MAX_USER_THREADS ||
      g_nonidle_threads_count >= g_max_threads || !passes_ub_test(C, T)) return -1; // invalid parameters or cannot create more threads
  tcb_t *tcb = &g_tcbs[prio]; // use the priority as the index for the TCB
  if (tcb->thread_state != THREAD_UNUSED || init_thread_context(tcb, fn, vargp) != 0)
    return -1; // failed to initialize thread context
  tcb->C = C;
  tcb->T = T;
  tcb->static_priority = (uint8_t)prio;
  tcb->dynamic_priority = (uint8_t)prio;
  tcb->curr_tick_count = 0;
  tcb->total_tick_count = 0;
  tcb->held_mutexes = 0;
  tcb->thread_state = THREAD_RUNNABLE;
  ++g_nonidle_threads_count;
  return 0;
}

int sys_scheduler_start(uint32_t frequency) {
  if (!g_threads_initialized || g_nonidle_threads_count == 0 || frequency == 0) return -1; // No threads to schedule or invalid frequency
  g_scheduler_freq = frequency;
  g_scheduler_running = true;
  g_current_tcb = g_default_tcb;
  g_current_tcb->thread_state = THREAD_RUNNING;
  g_sys_tick_count = 0;
  systick_init(frequency);
  pend_pendsv();
  return 0;
}

uint32_t sys_get_priority(void) { return g_current_tcb->dynamic_priority; }
uint32_t sys_get_time(void) { return g_sys_tick_count; }
uint32_t sys_thread_time(void) { return g_current_tcb->total_tick_count; }

void sys_thread_kill(void) {
  if (g_current_tcb == g_idle_tcb || g_current_tcb == g_default_tcb) return; // don't kill idle or default thread
  g_current_tcb->thread_state = THREAD_UNUSED; // mark the current thread as unused
  if (g_nonidle_threads_count != 0) --g_nonidle_threads_count; // decrement the count of non-idle threads
  g_current_tcb = g_default_tcb; // switch to the default thread
  pend_pendsv();
}

void sys_wait_until_next_period(void) { }

kmutex_t *sys_mutex_init(uint32_t max_prio) { (void)max_prio; return NULL; }
void sys_mutex_lock(kmutex_t *mutex) { (void)mutex; }
void sys_mutex_unlock(kmutex_t *mutex) { (void)mutex; }
