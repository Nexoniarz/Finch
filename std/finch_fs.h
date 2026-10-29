/* The runtime's folder functions, for std/fs.fch (they are in runtime/finch_rt.c, linked into every program). */
int finch_fs_is_dir(const char *path);
int finch_fs_exists(const char *path);
int finch_fs_mkdir(const char *path);
int finch_fs_remove_dir(const char *path);
void *finch_fs_list_open(const char *path);
const char *finch_fs_list_next(void *list);
void finch_fs_list_close(void *list);
const char *finch_fs_data_home(void);
