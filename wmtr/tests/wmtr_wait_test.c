/* wmtr_wait_test (in the VM guest, static, runs under FEX): a thread that waits a long time for a reply
 * must keep at most one WAIT command in flight. Each pending WAIT holds virtio-gpu control-queue
 * descriptors until its fence completes; re-submitting on every poll timeout filled the queue and
 * stalled every GPU client in the guest. The test waits ~2 s on memory only it updates (the host never
 * completes that fence) and counts WAIT submissions. Usage: wmtr_wait_test */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
int wmtr_debug_ping(void);
int wmtr_debug_wait(volatile uint64_t *done, uint64_t seq, uint32_t slot);
extern uint64_t wmtr_vgpu_wait_submits;
static volatile uint64_t done_flag;
static void *release(void *a) { (void)a; sleep(2); __atomic_store_n(&done_flag, 1, __ATOMIC_RELEASE); return NULL; }
int main(void) {
  if (wmtr_debug_ping()) { fprintf(stderr, "no wmtr transport\n"); return 2; }
  pthread_t t;
  pthread_create(&t, NULL, release, NULL);
  uint64_t before = wmtr_vgpu_wait_submits;
  wmtr_debug_wait(&done_flag, 1, 5);
  pthread_join(t, NULL);
  uint64_t n = wmtr_vgpu_wait_submits - before;
  printf("wmtr_wait_test: %llu WAIT submissions in a 2 s wait: %s\n", (unsigned long long)n, n == 1 ? "passed" : "FAILED");
  /* still answers after the long wait */
  if (wmtr_debug_ping()) { printf("wmtr_wait_test: ping after wait FAILED\n"); return 1; }
  return n == 1 ? 0 : 1;
}
