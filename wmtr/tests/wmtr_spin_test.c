/* wmtr_spin_test: the reply-wait spin (wmtr_spin.h). Calls that run on the host's blocking or compile
 * queues get no spin at all; other calls poll for at most the budget and then hand over to the
 * transport's blocking wait; polling makes no system call but the clock read (no sched_yield). */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
static unsigned yields;
#define sched_yield() (yields++, 0)
#include "wmtr_spin.h"

static volatile uint64_t done;
static void *
late_reply(void *arg) {
  usleep((useconds_t)(uintptr_t)arg);
  __atomic_store_n(&done, 1, __ATOMIC_RELEASE);
  return NULL;
}

int
main(void) {
  int fails = 0;
#define CHECK(c, m) do { if (!(c)) { fails++; printf("FAIL: %s\n", m); } } while (0)
  setenv("WMTR_SPIN_US", "200", 1);
  CHECK(wmtr_spin_budget_ns(2) == 0, "class B (nextDrawable, waitUntilCompleted) spins");
  CHECK(wmtr_spin_budget_ns(3) == 0, "class C (compiles) spins");
  CHECK(wmtr_spin_budget_ns(1) == 200000, "class S budget is not WMTR_SPIN_US");
  CHECK(wmtr_spin_budget_ns(0) == 200000, "class A budget");

  done = 1;
  CHECK(wmtr_spin_wait(&done, 1, 0), "an answered call is not seen without spinning");
  done = 0;
  uint64_t t0 = wmtr_now_ns();
  CHECK(!wmtr_spin_wait(&done, 1, 0), "budget 0 waited");
  CHECK(wmtr_now_ns() - t0 < 1000000, "budget 0 took over 1 ms");

  /* a reply after the budget: give up after about the budget, not thousands of iterations later */
  pthread_t th;
  pthread_create(&th, NULL, late_reply, (void *)(uintptr_t)20000);
  t0 = wmtr_now_ns();
  int got = wmtr_spin_wait(&done, 1, 200000);
  uint64_t dt = wmtr_now_ns() - t0;
  CHECK(!got, "a reply 20 ms late arrived within a 200 us budget");
  CHECK(dt >= 200000 && dt < 5000000, "spin did not stop near its budget");
  pthread_join(th, NULL);

  /* a reply within the budget is picked up by polling */
  done = 0;
  pthread_create(&th, NULL, late_reply, (void *)(uintptr_t)1000);
  got = wmtr_spin_wait(&done, 1, 2000000000ull);
  CHECK(got, "a reply within the budget was missed");
  pthread_join(th, NULL);
  CHECK(yields == 0, "the spin called sched_yield");
  /* one kick per executor sleep: the first client claims it, later ones until the host sleeps again do not */
  volatile uint32_t idle = 1;
  CHECK(wmtr_claim_kick(&idle), "first client does not kick a sleeping host");
  CHECK(!wmtr_claim_kick(&idle) && !wmtr_claim_kick(&idle), "a second client kicks again before the host slept again");
  idle = 1;
  CHECK(wmtr_claim_kick(&idle), "no kick after the host slept again");
  printf("wmtr_spin_test: %s\n", fails ? "FAILED" : "passed");
  return fails != 0;
}
