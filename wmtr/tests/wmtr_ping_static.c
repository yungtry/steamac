/* wmtr_ping_static: like wmtr_ping, linked with the client and a transport directly (static binary for
 * the guest, runs under FEX without a rootfs). Usage: wmtr_ping_static [N] */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
int wmtr_debug_ping(void);
static double now_us(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e6 + t.tv_nsec / 1e3; }
static int cmp(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }
int main(int argc, char **argv) {
  int n = argc > 1 ? (int)strtol(argv[1], NULL, 10) : 20000;
  double *v = malloc(n * sizeof *v);
  if (wmtr_debug_ping()) { fprintf(stderr, "first ping failed\n"); return 1; }
  for (int i = 0; i < 200; i++) wmtr_debug_ping();
  for (int i = 0; i < n; i++) { double a = now_us(); if (wmtr_debug_ping()) { fprintf(stderr, "ping failed\n"); return 1; } v[i] = now_us() - a; }
  qsort(v, n, sizeof *v, cmp);
  double s = 0; for (int i = 0; i < n; i++) s += v[i];
  printf("RESULT pings=%d avg_us=%.2f p50_us=%.2f p95_us=%.2f p99_us=%.2f max_us=%.1f\n", n, s / n, v[n / 2], v[n * 95 / 100], v[n * 99 / 100], v[n - 1]);
  return 0;
}
