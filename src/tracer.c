/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   tracer.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: everonel <everonel@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/07 11:39:38 by everonel          #+#    #+#             */
/*   Updated: 2026/05/13 10:24:50 by everonel         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "ft_strace.h"
# include "syscalls.h"
# include "signals.h"

/* Per-tracee entry-time state. Args from the entry-stop are printed at the
   matching exit-stop, so that pointer-target memory (output buffers, struct
   stat, etc.) is read after the kernel has filled it. */
typedef struct {
    long                       nr;
    unsigned long              argv[6];
    const syscall_info_t      *info;
    int                        prefix_printed;  /* EVENT_EXEC pre-printed the
                                                   "name(args)" half-line */
} pending_t;

static void print_prefix(pid_t pid, pending_t *p, long long retval) {
    if (p->prefix_printed) return;
    fprintf(stderr, "%s(", (p->info && p->info->name) ? p->info->name : "syscall_???");
    int n = p->info ? p->info->nargs : 0;
    for (int i = 0; i < n; i++) {
        if (i > 0) fprintf(stderr, ", ");
        format_arg(pid, p->info->args[i], p->argv[i], p->argv, i, retval);
    }
    fprintf(stderr, ")");
    p->prefix_printed = 1;
}

static int handle_syscall_stop(pid_t pid, int *in_syscall, pending_t *p) {
    struct user_regs_struct regs;
    struct iovec iov = { &regs, sizeof(regs) };
    if (ptrace(PTRACE_GETREGSET, pid, NT_PRSTATUS, &iov) < 0) {
        perror("ptrace GETREGSET");
        return -1;
    }
    if (!*in_syscall) {
        /* Entry: stash regs, print nothing. */
        p->nr = (long)regs.orig_rax;
        p->info = (p->nr >= 0 && (size_t)p->nr < syscalls_x86_64_count)
                      ? &syscalls_x86_64[p->nr] : NULL;
        p->argv[0] = regs.rdi; p->argv[1] = regs.rsi; p->argv[2] = regs.rdx;
        p->argv[3] = regs.r10; p->argv[4] = regs.r8;  p->argv[5] = regs.r9;
        p->prefix_printed = 0;
    } else {
        /* Exit: pointer-target memory is now kernel-filled; print full line. */
        long long retval = (long long)regs.rax;
        print_prefix(pid, p, retval);
        print_syscall_retval(p->info ? p->info->ret : ARG_LONG, retval);
        p->prefix_printed = 0;
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
    pending_t p = {0};

    if (attach_and_arm(tracee) < 0) return -1;

    for (;;) {
        pid_t w = waitpid(tracee, &status, 0);
        if (w < 0) {
            if (errno == EINTR) continue;
            perror("waitpid"); return -1;
        }

        if (WIFEXITED(status)) {
            if (in_syscall) {
                print_prefix(w, &p, 0);
                fprintf(stderr, " = ?\n");  /* e.g. exit_group never returns */
            }
            fprintf(stderr, "+++ exited with %d +++\n", WEXITSTATUS(status));
            break;
        }
        if (WIFSIGNALED(status)) {
            if (in_syscall) {
                print_prefix(w, &p, 0);
                fprintf(stderr, " = ?\n");
            }
            print_kill_trailer(WTERMSIG(status));
            break;
        }

        int event = status >> 16;
        int sig = 0;            /* signal to inject on resume */
        int use_listen = 0;     /* group-stop → PTRACE_LISTEN, not SYSCALL */

        if (event == PTRACE_EVENT_EXEC) {
            fprintf(stderr, "execve(...)");
            p.prefix_printed = 1;
            in_syscall = 1;
            seen_exec = 1;
        } else if (event == PTRACE_EVENT_STOP) {
            /* Under SEIZE, group-stops surface as EVENT_STOP with WSTOPSIG
               set to the stopping signal. EVENT_STOP also fires after a
               successful PTRACE_INTERRUPT (no real stopping signal); that
               one we just resume. Group-stops must use PTRACE_LISTEN —
               PTRACE_SYSCALL/CONT here would defeat the stop.
               Before seen_exec the pre-exec SIGSTOP from raise() leaks
               through as EVENT_STOP+SIGSTOP; treat it as a plain resume
               point, not a group-stop. */
            int stopsig = WSTOPSIG(status);
            if (seen_exec && (stopsig == SIGSTOP || stopsig == SIGTSTP
                              || stopsig == SIGTTIN || stopsig == SIGTTOU)) {
                use_listen = 1;
            }
        } else if (event != 0) {
            /* Other event-stops (EVENT_EXIT, etc.) — don't toggle, just resume. */
        } else if (WIFSTOPPED(status) && WSTOPSIG(status) == (SIGTRAP | 0x80)) {
            /* Syscall-stop. */
            if (seen_exec && handle_syscall_stop(w, &in_syscall, &p) < 0) {
                return -1;
            }
            /* else: pre-exec leftovers (tgkill from raise(SIGSTOP)) — silently swallow. */
        } else if (WIFSTOPPED(status)) {
            /* Signal-delivery-stop OR (pre-SEIZE-style) group-stop. The
               kernel distinguishes them by whether PTRACE_GETSIGINFO works:
               it returns EINVAL on a group-stop and succeeds otherwise.
               With SEIZE we normally get group-stops as EVENT_STOP above,
               but keep the GETSIGINFO check as a belt-and-braces fallback. */
            int stopsig = WSTOPSIG(status);
            siginfo_t si;
            if (ptrace(PTRACE_GETSIGINFO, w, 0, &si) < 0) {
                if (errno == EINVAL) {
                    use_listen = 1;
                } else if (errno != ESRCH) {
                    perror("ptrace GETSIGINFO");
                }
                /* On ESRCH the tracee is gone; the next waitpid reports it. */
            } else {
                /* Signal-delivery-stop: report and re-inject. Drop entirely
                   before exec — the pre-exec SIGSTOP from raise() is a
                   synchronization artifact, not a real signal; re-injecting
                   it would push the tracee into group-stop and deadlock. */
                if (seen_exec) {
                    print_signal_stop(stopsig, &si);
                    sig = stopsig;
                }
            }
        }

        long req = use_listen ? PTRACE_LISTEN : PTRACE_SYSCALL;
        if (ptrace(req, w, 0, sig) < 0 && errno != ESRCH) {
            perror(use_listen ? "ptrace LISTEN" : "ptrace SYSCALL");
            return -1;
        }
    }
    return 0;
}
