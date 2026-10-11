/* wmtr_ntdll_test (Linux): Wine loads ntdll.so RTLD_LOCAL, so the client's shared-event listener must
 * find NtSetEvent without the global scope. Loads a stand-in ntdll.so the same way, then the client.
 * Usage: wmtr_ntdll_test DIR/ntdll.so DIR/winemetal.so */
#include <dlfcn.h>
#include <stdio.h>
int main(int argc, char **argv) {
  if (argc < 3) return 2;
  void *nt = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
  if (!nt) { fprintf(stderr, "%s\n", dlerror()); return 1; }
  if (dlsym(RTLD_DEFAULT, "NtSetEvent")) { fprintf(stderr, "FAIL: stand-in leaked into the global scope\n"); return 1; }
  void *cl = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
  if (!cl) { fprintf(stderr, "%s\n", dlerror()); return 1; }
  void *(*find)(void) = (void *(*)(void))dlsym(cl, "wmtr_find_nt_set_event");
  if (!find) { fprintf(stderr, "FAIL: no wmtr_find_nt_set_event\n"); return 1; }
  int (*f)(void *, void *) = (int (*)(void *, void *))find();
  if (!f) { fprintf(stderr, "FAIL: NtSetEvent not found\n"); return 1; }
  if (f(NULL, NULL) != 0x1234) { fprintf(stderr, "FAIL: wrong NtSetEvent\n"); return 1; }
  printf("wmtr_ntdll_test: passed\n");
  return 0;
}
