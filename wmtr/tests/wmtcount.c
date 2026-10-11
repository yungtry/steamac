/* wmtcount: interposer for DXMT's winemetal unixlib. Install as winemetal.so next to the real
 * library renamed winemetal.real.so. Every unix call is forwarded unchanged; per call index it
 * counts calls and host time. At each present (presentDrawable*) it closes a "frame"; every
 * WMTCOUNT_PERIOD frames (default 300) it appends one line per active call index to
 * $WMTCOUNT_OUT (default /tmp/wmtcount.log): frames, calls/frame, us/call, plus total calls/frame.
 * Measurement only; it changes nothing the application sees. */
#include <dlfcn.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

typedef int32_t NTSTATUS;
typedef NTSTATUS (*unix_fn)(void *);

#define MAXF 160
static unix_fn real[MAXF];
static int nreal;
static _Atomic uint64_t cnt[MAXF], ns[MAXF];
static _Atomic uint64_t frames;
static int present_a = 47, present_b = 48; /* presentDrawable, presentDrawableAfterMinimumDuration */
static FILE *out;
static uint64_t period = 300;
static uint64_t t_last;
static pthread_mutex_t lk = PTHREAD_MUTEX_INITIALIZER;

static uint64_t now_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

static void dump(uint64_t nf) {
  pthread_mutex_lock(&lk);
  uint64_t t = now_ns();
  double wall = (t - t_last) / 1e9;
  uint64_t tot = 0, totns = 0;
  fprintf(out, "== frames=%llu wall_s=%.3f fps=%.1f\n", (unsigned long long)nf, wall, nf / wall);
  for (int i = 0; i < nreal; i++) {
    uint64_t c = atomic_exchange(&cnt[i], 0), n = atomic_exchange(&ns[i], 0);
    if (!c) continue;
    tot += c; totns += n;
    fprintf(out, "call %3d calls/frame=%9.2f us/call=%8.2f ms/frame=%7.3f\n", i, (double)c / nf, n / 1e3 / c,
            n / 1e6 / nf);
  }
  fprintf(out, "total calls/frame=%.1f host_ms/frame=%.3f\n", (double)tot / nf, totns / 1e6 / nf);
  fflush(out);
  t_last = t;
  pthread_mutex_unlock(&lk);
}

static NTSTATUS tramp(int i, void *p) {
  uint64_t a = now_ns();
  NTSTATUS s = real[i](p);
  atomic_fetch_add(&ns[i], now_ns() - a);
  atomic_fetch_add(&cnt[i], 1);
  if (i == present_a || i == present_b) {
    uint64_t f = atomic_fetch_add(&frames, 1) + 1;
    if (f % period == 0) dump(period);
  }
  return s;
}

#define T(n) static NTSTATUS t##n(void *p) { return tramp(n, p); }
#define T10(n) T(n##0) T(n##1) T(n##2) T(n##3) T(n##4) T(n##5) T(n##6) T(n##7) T(n##8) T(n##9)
T(0) T(1) T(2) T(3) T(4) T(5) T(6) T(7) T(8) T(9)
T10(1) T10(2) T10(3) T10(4) T10(5) T10(6) T10(7) T10(8) T10(9) T10(10) T10(11) T10(12) T10(13) T10(14) T10(15)
#define R(n) t##n,
#define R10(n) R(n##0) R(n##1) R(n##2) R(n##3) R(n##4) R(n##5) R(n##6) R(n##7) R(n##8) R(n##9)
static unix_fn tramps[MAXF] = {R(0) R(1) R(2) R(3) R(4) R(5) R(6) R(7) R(8) R(9) R10(1) R10(2) R10(3) R10(4) R10(5)
                                   R10(6) R10(7) R10(8) R10(9) R10(10) R10(11) R10(12) R10(13) R10(14) R10(15)};

__attribute__((visibility("default"))) const void *__wine_unix_call_funcs[MAXF];

__attribute__((constructor)) static void init(void) {
  Dl_info di;
  char path[4096];
  dladdr((void *)init, &di);
  snprintf(path, sizeof path, "%s", di.dli_fname);
  char *slash = strrchr(path, '/');
  snprintf(slash ? slash + 1 : path, sizeof path - (slash ? (slash + 1 - path) : 0), "winemetal.real.so");
  void *h = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
  if (!h) { fprintf(stderr, "wmtcount: dlopen %s: %s\n", path, dlerror()); abort(); }
  const void **tab = dlsym(h, "__wine_unix_call_funcs");
  if (!tab) { fprintf(stderr, "wmtcount: no table\n"); abort(); }
  /* the real table has no terminator; WMTCOUNT_N overrides the entry count */
  nreal = getenv("WMTCOUNT_N") ? atoi(getenv("WMTCOUNT_N")) : 146;
  if (nreal > MAXF) nreal = MAXF;
  for (int i = 0; i < nreal; i++) {
    real[i] = (unix_fn)tab[i];
    __wine_unix_call_funcs[i] = real[i] ? (const void *)tramps[i] : NULL;
  }
  const char *o = getenv("WMTCOUNT_OUT");
  out = fopen(o ? o : "/tmp/wmtcount.log", "a");
  if (!out) out = stderr;
  if (getenv("WMTCOUNT_PERIOD")) period = strtoull(getenv("WMTCOUNT_PERIOD"), NULL, 10);
  t_last = now_ns();
  fprintf(out, "wmtcount: %d entries from %s\n", nreal, path);
  fflush(out);
}
