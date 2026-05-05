/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   strace.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: everonel <everonel@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/29 15:26:07 by everonel          #+#    #+#             */
/*   Updated: 2026/05/05 12:48:51 by everonel         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "ft_strace.h"
# include "syscalls.h"

int strace (char *prog, char **args) {
    int     status;
    pid_t   tracee = fork();
    int     in_syscall = 0;
    int     seen_exec = 0;
    
    if (tracee < 0) { 
        perror("fork"); return -1; 
    }
    if (tracee == 0) {
        raise(SIGSTOP);
        execvp(prog, args);
        perror("execvp");
        _exit(127);
    }

    if (waitpid(tracee, &status, WUNTRACED) < 0) { perror("waitpid"); return -1; }

    if (ptrace(PTRACE_SEIZE, tracee, 0,
            PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACEEXEC | PTRACE_O_TRACEEXIT) < 0) {
        perror("ptrace SEIZE"); return -1;
    }

    // Lift the SIGSTOP-induced group-stop into a PTRACE_EVENT_STOP
    if (ptrace(PTRACE_INTERRUPT, tracee, 0, 0) < 0) {
        perror("ptrace INTERRUPT"); return -1;
    }

    // Consume the PTRACE_EVENT_STOP notification
    if (waitpid(tracee, &status, 0) < 0) { perror("waitpid"); return -1; }

    // Now we're in a normal ptrace-stop. Kick off syscall tracing.
    if (ptrace(PTRACE_SYSCALL, tracee, 0, 0) < 0) {
        perror("ptrace SYSCALL"); return -1;
    }


    for (;;) {
        pid_t w = waitpid(tracee, &status, 0);
        if (w < 0) { 
            if (errno == EINTR) continue;
            perror("waitpid"); return -1; 
        }

        if (WIFEXITED(status)) {
            if (in_syscall) printf(" = ?\n"); /* e.g. exit_group never returns */
            printf("+++ exited with %d +++\n", WEXITSTATUS(status));
            break;
        }
        if (WIFSIGNALED(status)) {
            if (in_syscall) printf(" = ?\n");
            printf("+++ killed by signal %d +++\n", WTERMSIG(status));
            break;
        }

        int event = status >> 16;
        int sig = 0;

        if (event == PTRACE_EVENT_EXEC) {
            /* Print execve's entry now (we suppressed it pre-exec) so the
             * upcoming syscall-exit-stop closes the line with " = 0". */
            printf("execve(...)");
            fflush(stdout);
            in_syscall = 1;
            seen_exec = 1;
        } else if (event != 0) {
            /* Other event-stops (EVENT_EXIT, etc.) — don't toggle, just resume. */
        } else if (WIFSTOPPED(status) && WSTOPSIG(status) == (SIGTRAP | 0x80)) {
            /* Real syscall-stop. */
            if (seen_exec) {
                struct user_regs_struct regs;
                struct iovec iov = { &regs, sizeof(regs) };
                if (ptrace(PTRACE_GETREGSET, w, NT_PRSTATUS, &iov) < 0) {
                    perror("ptrace GETREGSET"); return -1;
                }
                if (!in_syscall) {
                    const char *name = syscalls_x86_64[regs.orig_rax].name;
                    printf("%s(...)", name ? name : "syscall_???");
                    fflush(stdout);
                } else {
                    long long retval = (long long)regs.rax;
                    if ((unsigned long long)retval > -4096ULL) {
                        int err = -retval;
                        const char *n = strerrorname_np(err);
                        printf(" = -1 %s (%s)\n", n ? n : "?", strerror(err));
                    } else {
                        printf(" = %lld\n", retval);
                    }
                }
                in_syscall = !in_syscall;
            }
            /* else: pre-exec leftovers (tgkill from raise(SIGSTOP)) — silently swallow. */
        }
        /* else: signal-delivery-stop or group-stop. Milestone 4 territory; for now swallow. */

        if (ptrace(PTRACE_SYSCALL, w, sig, 0) < 0 && errno != ESRCH) {
            perror("ptrace SYSCALL"); return -1;
        }
    }
    return 0;
}
