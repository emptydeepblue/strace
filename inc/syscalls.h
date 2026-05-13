#ifndef SYSCALLS_H
# define SYSCALLS_H

# include <stddef.h>
# include <sys/types.h>   /* pid_t */

typedef enum {
    ARG_NONE = 0,
    ARG_INT, 
    ARG_LONG, 
    ARG_SIZE,
    ARG_FD, 
    ARG_DIRFD,
    ARG_MODE,
    ARG_FLAGS,
    ARG_SIGNUM,
    ARG_PTR, 
    ARG_STR,
    ARG_BUF,
    ARG_OPEN_FLAGS,
    ARG_PROT,
    ARG_MAP_FLAGS,
    ARG_ACCESS_MODE,
    ARG_AT_FLAGS,
    ARG_STAT,         /* struct stat * (output) — decoded on success */
    ARG_TIMESPEC,     /* struct timespec * (in or out) — always decoded */
} arg_type_t;

typedef struct {
    const char  *name;
    int         nargs;
    arg_type_t  args[6];
} syscall_info_t;

extern const syscall_info_t syscalls_x86_64[];
extern const size_t         syscalls_x86_64_count;

extern const syscall_info_t syscalls_i386[];
extern const size_t         syscalls_i386_count;


int format_arg(pid_t pid, arg_type_t type, unsigned long val,
               const unsigned long *args, int idx, long long retval);

/* decode_struct.c — pretty-printers for stat/timespec/etc. */
int print_stat(pid_t pid, unsigned long ptr, long long retval);
int print_timespec(pid_t pid, unsigned long ptr);

#endif