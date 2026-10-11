/* wmtr_ping: round-trip latency of synchronous no-op requests through a wmtr client library and its
 * transport (e.g. the macOS socket transport to wmtr-server). Usage: wmtr_ping path/to/winemetal.so [N] */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static double now_us(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC_RAW, &t); return t.tv_sec * 1e6 + t.tv_nsec / 1e3; }
static int cmp(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }
int main(int argc, char **argv) {
  void *h = dlopen(argv[1], RTLD_NOW);
  if (!h) { fprintf(stderr, "%s\n", dlerror()); return 1; }
  int (*ping)(void) = (int (*)(void))dlsym(h, "wmtr_debug_ping");
  int n = argc > 2 ? atoi(argv[2]) : 20000;
  double *v = malloc(n * sizeof *v);
  for (int i = 0; i < 200; i++) ping();
  for (int i = 0; i < n; i++) { double a = now_us(); if (ping()) { fprintf(stderr, "ping failed\n"); return 1; } v[i] = now_us() - a; }
  qsort(v, n, sizeof *v, cmp);
  double s = 0; for (int i = 0; i < n; i++) s += v[i];
  printf("RESULT pings=%d avg_us=%.2f p50_us=%.2f p95_us=%.2f p99_us=%.2f max_us=%.1f\n", n, s / n, v[n / 2], v[n * 95 / 100], v[n * 99 / 100], v[n - 1]);
  return 0;
}
