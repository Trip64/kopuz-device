/*
 * Minimal syscall stubs for newlib-nano (nano.specs)
 * Required for snprintf, malloc, etc.
 */
#include <sys/stat.h>
#include <errno.h>

int _close(int fd) { (void)fd; return -1; }
int _fstat(int fd, struct stat *st) { (void)fd; st->st_mode = S_IFCHR; return 0; }
int _isatty(int fd) { (void)fd; return 1; }
int _lseek(int fd, int ptr, int dir) { (void)fd; (void)ptr; (void)dir; return 0; }
int _read(int fd, char *ptr, int len) { (void)fd; (void)ptr; (void)len; return 0; }
int _write(int fd, char *ptr, int len) { (void)fd; (void)ptr; return len; }
void _kill(int pid, int sig) { (void)pid; (void)sig; }
int _getpid(void) { return 1; }
void _exit(int status) { (void)status; while(1) {} }

extern char _end; /* defined by linker */
static char *heap_ptr = 0;
void *_sbrk(int incr) {
    if (heap_ptr == 0) heap_ptr = &_end;
    char *prev = heap_ptr;
    heap_ptr += incr;
    return prev;
}
