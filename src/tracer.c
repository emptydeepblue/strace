/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   tracer.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: everonel <everonel@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/07 11:39:38 by everonel          #+#    #+#             */
/*   Updated: 2026/05/07 14:55:55 by everonel         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "ft_strace.h"
# include "syscalls.h"

static int handle_syscall_stop(pid_t pid, int *in_syscall) {
    struct user_regs_struct regs;
    struct iovec iov = { &regs, sizeof(regs) };
    if (ptrace(PTRACE_GETREGSET, pid, NT_PRSTATUS, &iov) < 0) {
        perror("ptrace GETREGSET");
        return -1;
    }
    if (!*in_syscall) {
        unsigned long nr = regs.orig_rax;
        const syscall_info_t *info = (nr < syscalls_x86_64_count) ? &syscalls_x86_64[nr] : NULL;
        const unsigned long argv[6] = { regs.rdi, regs.rsi, regs.rdx, regs.r10, regs.r8, regs.r9 };
        fprintf(stderr, "%s(", (info && info->name) ? info->name : "syscall_???");
        int n = info ? info->nargs : 0;
        for (int i = 0; i < n; i++) {
            if (i > 0) fprintf(stderr, ", ");
            format_arg(pid, info->args[i], argv[i], argv, i);
        }
        fprintf(stderr, ")");
    } else {
        print_syscall_retval((long long)regs.rax);
    }
    *in_syscall = !*in_syscall;
    return 0;
}

static int attach_and_arm(pid_t tracee) {
    int status;

    if (waitpid(tracee, &status, WUNTRACED) < 0) {
        perror("waitpid"); return -1;
    }
    if (ptrace(PTRACE_SEIZE, tracee, 0,
               PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACEEXEC | PTRACE_O_TRACEEXIT) < 0) {
        perror("ptrace SEIZE"); return -1;
    }
    if (ptrace(PTRACE_INTERRUPT, tracee, 0, 0) < 0) {
        perror("ptrace INTERRUPT"); return -1;
    }
    if (waitpid(tracee, &status, 0) < 0) {
        perror("waitpid"); return -1;
    }
    if (ptrace(PTRACE_SYSCALL, tracee, 0, 0) < 0) {
        perror("ptrace SYSCALL"); return -1;
    }
    return 0;
}

int run_tracer(pid_t tracee) {
    int status;
    int in_syscall = 0;
    int seen_exec = 0;
    
    if (attach_and_arm(tracee) < 0) return -1;

    for (;;) {
        pid_t w = waitpid(tracee, &status, 0);
        if (w < 0) { 
            if (errno == EINTR) continue;
            perror("waitpid"); return -1; 
        }

        if (WIFEXITED(status)) {
            if (in_syscall) fprintf(stderr, " = ?\n"); /* e.g. exit_group never returns */
            fprintf(stderr, "+++ exited with %d +++\n", WEXITSTATUS(status));
            break;
        }
        if (WIFSIGNALED(status)) {
            if (in_syscall) fprintf(stderr, " = ?\n");
            fprintf(stderr, "+++ killed by signal %d +++\n", WTERMSIG(status));
            break;
        }

        int event = status >> 16;
        int sig = 0;

        if (event == PTRACE_EVENT_EXEC) {
            fprintf(stderr, "execve(...)");
            in_syscall = 1;
            seen_exec = 1;
        } else if (event != 0) {
            /* Other event-stops (EVENT_EXIT, etc.) — don't toggle, just resume. */
        } else if (WIFSTOPPED(status) && WSTOPSIG(status) == (SIGTRAP | 0x80)) {
            /* Real syscall-stop. */
            if (seen_exec && handle_syscall_stop(w, &in_syscall) < 0) {
                return -1;
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