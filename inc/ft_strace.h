#ifndef FT_STRACE_H
# define FT_STRACE_H
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <errno.h>
# include <signal.h>
# include <elf.h>

# include <sys/ptrace.h>
# include <sys/wait.h>
# include <sys/user.h>
# include <sys/uio.h>

int strace (char *prog, char **args);
#endif