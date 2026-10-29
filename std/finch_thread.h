/* The runtime's thread functions, for std/thread.fch (they are in runtime/finch_rt.c, linked into every program). */
void *finch_thread_spawn(void (*fn)(void *), void *arg);
int finch_thread_join(void *thread);
void *finch_mutex_new(void);
void finch_mutex_lock(void *mutex);
void finch_mutex_unlock(void *mutex);
void finch_mutex_free(void *mutex);
int finch_cpu_count(void);
