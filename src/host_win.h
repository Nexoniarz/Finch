/* The console functions src/host_windows.fch needs. Declared here, not by including <windows.h>:
   libclang can't parse that header with MinGW's intrinsics. All are in kernel32. */
void *GetStdHandle(unsigned long which);
int GetConsoleMode(void *console, unsigned long *mode);
int SetConsoleMode(void *console, unsigned long mode);
unsigned long long GetTickCount64(void);
