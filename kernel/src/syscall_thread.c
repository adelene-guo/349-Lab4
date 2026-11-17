#include <stdint.h>
#include <stdbool.h>
#include "syscall_thread.h"
#include "syscall_mutex.h"
#include <arm.h>
#include <kernel.h>
#include <context_switch.h>
#include <systick.h>
#include <mpu.h>
#include <syscall.h>
#include <printk.h>

// #define NUM_TCBS THREAD_TCB_COUNT
// #define MAX_USER_THREADS 14 moved to syscall_thread.h
#define IDLE_THREAD_INDEX 14
#define DEFAULT_THREAD_INDEX 15
#define MIN_STACK_WORDS 256

#define SYSTICK_ENABLE 1

float ub_table[] = {
  0.000, 1.000, .8284, .7798, .7568,
  .7435, .7348, .7286, .7241, .7205,
  .7177, .7155, .7136, .7119, .7106,
  .7094, .7083, .7075, .7066, .7059,
  .7052, .7047, .7042, .7037, .7033,
  .7028, .7025, .7021, .7018, .7015,
  .7012, .7009
};

// moved to header file
tcb_t g_tcbs[NUM_TCBS];
tcb_t *g_current_tcb;
tcb_t *g_idle_tcb;
tcb_t *g_default_tcb;
uint32_t g_nonidle_threads_count;
uint32_t g_max_threads;
uint32_t g_max_mutexes;
bool g_threads_initialized;
volatile bool g_scheduler_running;
uint32_t g_scheduler_freq;
volatile uint32_t g_sys_tick_count;
uint32_t stack_words;

uint32_t volatile mutex_index;
kmutex_t mutexes[MAX_MUTEX];

extern void default_idle(void);

/** @brief Checks if a thread passes the utilization bound test */
static bool passes_ub_test(uint32_t new_C, uint32_t new_T) {
  if (new_T == 0) return false;
  float utilization = (float)new_C / (float)new_T;
  uint32_t count = 1;
  for (uint32_t i = 0; i < MAX_USER_THREADS; i++) {
    if (g_tcbs[i].thread_state == THREAD_UNUSED) continue;
    if (g_tcbs[i].T == 0) return false;
    utilization += (float)g_tcbs[i].C / (float)g_tcbs[i].T;
    count++;
  }
  return count < sizeof(ub_table) / sizeof(ub_table[0]) && utilization <= ub_table[count];
}

/** @brief Picks the next thread to run */
tcb_t *pick_next_thread(void) {
  tcb_t *best = g_idle_tcb;
  uint32_t best_prio = IDLE_THREAD_INDEX;
  for (uint32_t i = 0; i < MAX_USER_THREADS; i++) {
    tcb_t *thread = &g_tcbs[i];
    if (thread->thread_state != THREAD_RUNNABLE &&
        thread->thread_state != THREAD_RUNNING) continue;
    if (thread->dynamic_priority <= best_prio) {
      best = thread;
      best_prio = thread->dynamic_priority;
    }
  }
  
  return best;

  // /** round-robin scheduling */
  // uint32_t start = (uint32_t)(g_current_tcb - g_tcbs); // get the index of the current thread
  // for (uint32_t offset = 1; offset <= NUM_TCBS; ++offset) {
  //   tcb_t *candidate = &g_tcbs[(start + offset) % NUM_TCBS]; // find next thread with static priority
  //   if (candidate->thread_state == THREAD_RUNNABLE) return candidate;
  // }

  // return g_idle_tcb;
  // /** rms scheduling */
  // for (uint32_t i=0; i<MAX_USER_THREADS; i++){
  //   tcb_t *thread = &g_tcbs[i];
  //   if (thread==g_idle_tcb || thread==g_default_tcb) continue;

  //   thread_state_t thread_state = thread->thread_state;
  //   if (thread_state==THREAD_RUNNABLE || thread_state==THREAD_RUNNING) return thread;

  // }

  //   g_idle_tcb->thread_state = THREAD_RUNNING; //no other runnable threads
  //   g_current_tcb = g_idle_tcb;
  //   return g_idle_tcb;

}

void *pendsv_c_handler(void *context_ptr) {
  tcb_t *prev = g_current_tcb;
  // save the current thread's context
  prev->msp_base = context_ptr;
  prev->psp_base = get_psp();
  prev->svc_active = get_svc_status() != 0;
  if (prev == g_idle_tcb && prev->thread_state == THREAD_UNUSED) {
    // reinitialize idle thread if it was killed
    if (init_thread_context(prev, (void *)default_idle, NULL) == 0) {
      prev->static_priority = IDLE_THREAD_INDEX;
      prev->dynamic_priority = IDLE_THREAD_INDEX;
      prev->thread_state = THREAD_RUNNABLE;
    }
  } else if (prev->thread_state == THREAD_RUNNING && prev != g_default_tcb) {
    // set running, non-default threads to runnable
    prev->thread_state = THREAD_RUNNABLE;
  }

  tcb_t *next;
  if (g_scheduler_running && g_nonidle_threads_count == 0) {
    // default thread if no non-idle threads are runnable
    g_scheduler_running = false;
    systick_stop();
    g_default_tcb->thread_state = THREAD_RUNNABLE;
    next = g_default_tcb;
  } else {
    next = pick_next_thread();
    // pick the next thread to run
  }
  next->thread_state = THREAD_RUNNING;
  // set the next thread to running
  g_current_tcb = next;
  set_psp(next->psp_base);
  set_svc_status(next->svc_active);
  return next->msp_base;
}

int sys_thread_init(uint32_t max_threads, uint32_t stack_size, void *idle_fn,
                    uint32_t max_mutexes) {
  if (max_threads == 0 || max_threads > MAX_USER_THREADS) return -1;
  // invalid number of threads
  uint32_t words = stack_size < MIN_STACK_WORDS ? MIN_STACK_WORDS : stack_size;
  // ensure minimum stack size
  words = 1U << mm_log2ceil_size(words);
  mutex_index = 0;
  uint32_t user_region_words = (uint32_t)(__thread_u_stacks_top - __thread_u_stacks_low);
  uint32_t kernel_region_words = (uint32_t)(__thread_k_stacks_top - __thread_k_stacks_low);
  if ((max_threads + 1U) * words > user_region_words ||
      (max_threads + 1U) * words > kernel_region_words) return -1;
      // insufficient memory
  for (uint32_t i = 0; i < NUM_TCBS; i++) {
    // initialize all TCBs to unused state
    g_tcbs[i].thread_state = THREAD_UNUSED;
    g_tcbs[i].stack_slot = THREAD_NO_STACK_SLOT;
    g_tcbs[i].msp_base = NULL;
    g_tcbs[i].psp_base = NULL;
    g_tcbs[i].svc_active = false;
    g_tcbs[i].C = 0;
    g_tcbs[i].T = 0;
    g_tcbs[i].curr_tick_count = 0;
    g_tcbs[i].total_tick_count = 0;
    g_tcbs[i].next_wake_up = 0;
    g_tcbs[i].held_mutexes = 0;
  }
  g_max_threads = max_threads;
  g_max_mutexes = max_mutexes;
  stack_words = words;
  g_nonidle_threads_count = 0;
  g_scheduler_running = false;
  g_idle_tcb = &g_tcbs[IDLE_THREAD_INDEX];
  g_default_tcb = &g_tcbs[DEFAULT_THREAD_INDEX];
  g_current_tcb = g_default_tcb;

  void *idle = idle_fn != NULL ? idle_fn : (void *)default_idle;
  // use default idle function if none provided
  if (init_thread_context(g_idle_tcb, idle, NULL) != 0) return -1;
  // failed to initialize idle thread context
  g_idle_tcb->static_priority = IDLE_THREAD_INDEX;
  g_idle_tcb->dynamic_priority = IDLE_THREAD_INDEX;
  g_idle_tcb->thread_state = THREAD_RUNNABLE;
  g_threads_initialized = true;
  return 0;
}

int sys_thread_create(void *fn, uint32_t prio, uint32_t C, uint32_t T, void *vargp) {
  int interrupt_state = save_interrupt_state_and_disable();
  if (!g_threads_initialized || prio >= MAX_USER_THREADS ||
      g_nonidle_threads_count >= g_max_threads || !passes_ub_test(C, T)) { // invalid parameters or cannot create more threads
    restore_interrupt_state(interrupt_state);
    return -1; // invalid parameters or cannot create more threads
  }
  tcb_t *tcb = &g_tcbs[prio]; // use the priority as the index for the TCB
  if (tcb->thread_state != THREAD_UNUSED || init_thread_context(tcb, fn, vargp) != 0) { // failed to initialize thread context
    restore_interrupt_state(interrupt_state);
    return -1; // failed to initialize thread context
  }
  tcb->C = C;
  tcb->T = T;
  tcb->static_priority = (uint8_t)prio;
  tcb->dynamic_priority = (uint8_t)prio;
  tcb->curr_tick_count = 0;
  tcb->total_tick_count = 0;
  tcb->next_wake_up = g_scheduler_running ? g_sys_tick_count+T : T;
  tcb->held_mutexes = 0;
  tcb->thread_state = THREAD_RUNNABLE;
  g_nonidle_threads_count++;
  bool scheduler_running = g_scheduler_running;
  restore_interrupt_state(interrupt_state);
  if (scheduler_running) pend_pendsv(); // trigger context switch
  return 0;
}

int sys_scheduler_start(uint32_t frequency) {
  if (!g_threads_initialized || g_nonidle_threads_count == 0 || frequency == 0) return -1; // No threads to schedule or invalid frequency
  g_scheduler_freq = frequency;
  g_scheduler_running = true;
  g_current_tcb = g_default_tcb;
  g_current_tcb->thread_state = THREAD_WAITING;
  g_sys_tick_count = 0;
  systick_init(frequency);
  pend_pendsv();
  return 0;
}

uint32_t sys_get_priority(void) { return g_current_tcb->dynamic_priority; }
uint32_t sys_get_time(void) { return g_sys_tick_count; }
uint32_t sys_thread_time(void) { return g_current_tcb->total_tick_count; }

void sys_thread_kill(void) {
  if (g_current_tcb == g_default_tcb) { // sys exit if you kill the default thread
    sys_exit(0);
    return;
  }

  if (g_current_tcb == g_idle_tcb) { // default thread is scheduled to run
    int interrupt_state = save_interrupt_state_and_disable();
    g_idle_tcb->thread_state = THREAD_UNUSED;
    pend_pendsv();
    restore_interrupt_state(interrupt_state);
    return;
  }

  int interrupt_state = save_interrupt_state_and_disable(); //kill a normal thread and reschedule
  g_current_tcb->thread_state = THREAD_UNUSED;
  g_current_tcb->stack_slot = THREAD_NO_STACK_SLOT;
  for (uint32_t i = 0; i < g_max_mutexes; i++) {
    if ((g_current_tcb->held_mutexes >> i) & 1) mutexes[i].locked_by = NOT_LOCKED;
  }
  g_current_tcb->held_mutexes = 0;
  if (g_nonidle_threads_count != 0) --g_nonidle_threads_count;
  pend_pendsv();
  restore_interrupt_state(interrupt_state);
  return;
}

 void sys_wait_until_next_period(void) {
  if (!g_scheduler_running || g_current_tcb == NULL ||
      g_current_tcb == g_idle_tcb || g_current_tcb == g_default_tcb) return;

  int interrupt_state = save_interrupt_state_and_disable();
  if (g_current_tcb->held_mutexes != 0) {
    printk("ERROR: Thread waiting while holding mutexes\n");
  }
  g_current_tcb->thread_state = THREAD_WAITING;
  pend_pendsv();
  restore_interrupt_state(interrupt_state);
}

kmutex_t *sys_mutex_init(uint32_t max_prio) {
  if (mutex_index >= g_max_mutexes) return NULL;
  kmutex_t *new_mutex = &mutexes[mutex_index];
  new_mutex->locked_by = NOT_LOCKED;
  new_mutex->prio_ceil = max_prio;
  new_mutex->index = mutex_index;
  mutex_index++;
  return new_mutex;
}

void sys_mutex_lock(kmutex_t *mutex) {
  int interrupt_state = save_interrupt_state_and_disable();

  if (mutex->locked_by == g_current_tcb->static_priority){
    printk("ERROR: Mutex already locked\n");
    restore_interrupt_state(interrupt_state);
    return;
  }

  if (g_current_tcb == g_idle_tcb) {
    printk("ERROR:Mutex used by idle thread\n");
    restore_interrupt_state(interrupt_state);
    return;
  }

  if (g_current_tcb->static_priority < mutex->prio_ceil) {
    printk("ERROR: Mutex ceiling violated\n");
    restore_interrupt_state(interrupt_state);
    sys_thread_kill();
    return;
  }

  while (mutex->locked_by != NOT_LOCKED) {
    pend_pendsv();
    restore_interrupt_state(interrupt_state);
    interrupt_state = save_interrupt_state_and_disable();
  }

  mutex->locked_by = g_current_tcb->static_priority;
  g_current_tcb->held_mutexes |= 1 << mutex->index;
  if (g_current_tcb->dynamic_priority > mutex->prio_ceil) {
    g_current_tcb->dynamic_priority = mutex->prio_ceil;
  }
  restore_interrupt_state(interrupt_state);
  return;

}

void sys_mutex_unlock(kmutex_t *mutex) {
  int interrupt_state = save_interrupt_state_and_disable();

  if (mutex->locked_by == NOT_LOCKED) {
    printk("ERROR: Mutex is not locked\n");
    restore_interrupt_state(interrupt_state);
    return;
  }

  if (mutex->locked_by != g_current_tcb->static_priority) {
    printk("ERROR:Mutex is not in thread's control\n");
    restore_interrupt_state(interrupt_state);
    return;
  }

  mutex->locked_by = NOT_LOCKED;
  g_current_tcb->held_mutexes &= ~(1 << mutex->index);
  g_current_tcb->dynamic_priority = g_current_tcb->static_priority;
  for (uint32_t i = 0; i < g_max_mutexes; i++) {
    bool thread_owns_mutex = (g_current_tcb->held_mutexes >> i) & 1;
    if (thread_owns_mutex) {
      if (g_current_tcb->dynamic_priority > mutexes[i].prio_ceil) {
        g_current_tcb->dynamic_priority = mutexes[i].prio_ceil;
      }
    }
  }
  pend_pendsv();
  restore_interrupt_state(interrupt_state);
  return;
}