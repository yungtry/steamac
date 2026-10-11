/* How long a client thread polls its reply word before it blocks in the transport.
 *
 * Polling only pays off for calls the host answers within microseconds. Calls the host runs on its
 * blocking or compile queues (class B: nextDrawable, waitUntilCompleted, waitUntilSignaledValue,
 * listener start; class C: library/pipeline/shader compiles) and the event listener's wait take
 * milliseconds or longer: those block at once. Everything else polls for at most the budget
 * (WMTR_SPIN_US, default 20 us), with a CPU pause between loads and no system calls. Inside the VM
 * every guest instruction of a spinning thread is a busy vCPU on the host, and a sched_yield is a
 * system call per iteration under FEX; the previous 4000-iteration spin with sched_yield kept vCPUs
 * busy for the whole of each multi-millisecond wait. */
#ifndef WMTR_SPIN_H
#define WMTR_SPIN_H
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#define WMTR_SPIN_US_DEFAULT 20

static inline void
wmtr_cpu_relax(void) {
#if defined(__x86_64__) || defined(__i386__)
  __builtin_ia32_pause();
#elif defined(__aarch64__)
  __asm__ __volatile__("yield");
#endif
}

static inline uint64_t
wmtr_now_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* spin budget for a call of class `cls` (WMTR_CLASS_*: 0 A, 1 S, 2 B, 3 C), from WMTR_SPIN_US once */
static inline uint64_t
wmtr_spin_budget_ns(int cls) {
  static uint64_t budget = ~0ull;
  if (cls == 2 || cls == 3) return 0;
  if (budget == ~0ull) {
    /* parsed by hand: strtoul binds to glibc 2.38's __isoc23_strtoul with a new compiler */
    const char *e = getenv("WMTR_SPIN_US");
    uint64_t us = WMTR_SPIN_US_DEFAULT;
    if (e && *e >= '0' && *e <= '9')
      for (us = 0; *e >= '0' && *e <= '9'; e++) us = us * 10 + (uint64_t)(*e - '0');
    budget = us * 1000ull;
  }
  return budget;
}

/* poll *done >= seq for at most budget_ns; returns 1 when it got there */
static inline int
wmtr_spin_wait(volatile uint64_t *done, uint64_t seq, uint64_t budget_ns) {
  if (__atomic_load_n(done, __ATOMIC_ACQUIRE) >= seq) return 1;
  if (!budget_ns) return 0;
  uint64_t end = wmtr_now_ns() + budget_ns;
  for (unsigned i = 1;; i++) {
    wmtr_cpu_relax();
    if (__atomic_load_n(done, __ATOMIC_ACQUIRE) >= seq) return 1;
    if (!(i & 63) && wmtr_now_ns() >= end) return 0;
  }
}

/* Whether this client must kick the sleeping host executor. The first client to see host_idle clears it and
 * kicks; requests written before the host has woken up then go without a kick of their own. Each kick is a
 * virtio-gpu submission and a VM exit; in VRChat the host slept ~97 times and was kicked ~290 times a frame. The
 * executor sets host_idle again before it next sleeps. */
static inline int
wmtr_claim_kick(volatile uint32_t *host_idle) {
  return __atomic_exchange_n(host_idle, 0, __ATOMIC_SEQ_CST) != 0;
}

#endif
