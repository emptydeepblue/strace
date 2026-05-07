#ifndef SYSCALLS_H
# define SYSCALLS_H

# include <stddef.h>
# include <sys/types.h>   /* pid_t */

typedef enum {
    ARG_NONE = 0,
    ARG_INT, 
    ARG_LONG, 
    ARG_PTR, 
    ARG_FD, 
    ARG_STR,
    ARG_BUF, 
    ARG_SIZE, 
    ARG_OFLAGS, 
    ARG_MODE, 
    ARG_DIRFD
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
               const unsigned long *args, int idx);

#endif