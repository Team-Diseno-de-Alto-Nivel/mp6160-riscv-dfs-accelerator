#include <errno.h>
#include <stddef.h>
#include <sys/stat.h>

#define UART0_BASE       0x10000000UL
#define SIFIVE_TEST_BASE 0x00100000UL

#define SIFIVE_TEST_PASS 0x5555U
#define SIFIVE_TEST_FAIL 0x3333U

static volatile unsigned char *const uart = (volatile unsigned char *)UART0_BASE;
static volatile unsigned int *const test = (volatile unsigned int *)SIFIVE_TEST_BASE;

extern char _end;
static char *heap_ptr = &_end;

char *__env[1] = {0};
char **environ = __env;

void *__dso_handle = 0;

void _exit(int code) {
    test[0] = code == 0 ? SIFIVE_TEST_PASS
                        : (((unsigned int)code << 16) | SIFIVE_TEST_FAIL);
    for (;;) {
    }
}

int _write(int fd, const char *buf, int len) {
    (void)fd;
    for (int i = 0; i < len; ++i) {
        uart[0] = (unsigned char)buf[i];
    }
    return len;
}

void *_sbrk(ptrdiff_t incr) {
    char *prev = heap_ptr;
    heap_ptr += incr;
    return prev;
}

int _read(int fd, char *buf, int len) {
    (void)fd;
    (void)buf;
    (void)len;
    return 0;
}

int _close(int fd) {
    (void)fd;
    return -1;
}

int _fstat(int fd, struct stat *st) {
    (void)fd;
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int fd) {
    (void)fd;
    return 1;
}

int _lseek(int fd, int offset, int whence) {
    (void)fd;
    (void)offset;
    (void)whence;
    return 0;
}

int _kill(int pid, int sig) {
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

int _getpid(void) {
    return 1;
}
