/* The Windows functions std/time_windows.fch needs. Declared here, not by including <windows.h>:
   libclang can't parse that header with MinGW's intrinsics. Both are in kernel32. */
unsigned long long GetTickCount64(void);
void Sleep(unsigned long milliseconds);
int QueryPerformanceCounter(long long *count);
int QueryPerformanceFrequency(long long *frequency);
