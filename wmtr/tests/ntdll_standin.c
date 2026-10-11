/* stand-in for Wine's ntdll.so in wmtr_ntdll_test */
int NtSetEvent(void *handle, void *prev) { (void)handle; (void)prev; return 0x1234; }
