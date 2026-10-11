/* Linux guest client: Proton runs unixlibs inside the Steam Runtime (sniper, glibc 2.31). Bind the symbols
 * that a newer build glibc would version 2.34+ to their original versions so the library loads there. */
#if defined(__linux__) && defined(__x86_64__)
__asm__(".symver pthread_once,pthread_once@GLIBC_2.2.5");
__asm__(".symver pthread_key_create,pthread_key_create@GLIBC_2.2.5");
__asm__(".symver pthread_getspecific,pthread_getspecific@GLIBC_2.2.5");
__asm__(".symver pthread_setspecific,pthread_setspecific@GLIBC_2.2.5");
__asm__(".symver dlsym,dlsym@GLIBC_2.2.5");
__asm__(".symver dlopen,dlopen@GLIBC_2.2.5");
#endif
