/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   signals.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: everonel <everonel@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/13 11:00:00 by everonel          #+#    #+#             */
/*   Updated: 2026/05/13 11:00:00 by everonel         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "ft_strace.h"
# include "signals.h"

static void print_signame(int sig) {
    const char *n = sigabbrev_np(sig);
    if (n) fprintf(stderr, "SIG%s", n);
    else   fprintf(stderr, "%d", sig);
}

/* Resolve si_code to its symbolic name. Negative codes are signal-agnostic;
   positive ones are per-signal. Returns NULL when unknown — caller prints
   the numeric value as a fallback. */
static const char *si_code_name(int signo, int code) {
    switch (code) {
        case SI_USER:    return "SI_USER";
        case SI_KERNEL:  return "SI_KERNEL";
        case SI_QUEUE:   return "SI_QUEUE";
        case SI_TIMER:   return "SI_TIMER";
        case SI_MESGQ:   return "SI_MESGQ";
        case SI_ASYNCIO: return "SI_ASYNCIO";
        case SI_SIGIO:   return "SI_SIGIO";
        case SI_TKILL:   return "SI_TKILL";
    }
    if (signo == SIGCHLD) {
        switch (code) {
            case CLD_EXITED:    return "CLD_EXITED";
            case CLD_KILLED:    return "CLD_KILLED";
            case CLD_DUMPED:    return "CLD_DUMPED";
            case CLD_TRAPPED:   return "CLD_TRAPPED";
            case CLD_STOPPED:   return "CLD_STOPPED";
            case CLD_CONTINUED: return "CLD_CONTINUED";
        }
    } else if (signo == SIGSEGV) {
        switch (code) {
            case SEGV_MAPERR: return "SEGV_MAPERR";
            case SEGV_ACCERR: return "SEGV_ACCERR";
        }
    } else if (signo == SIGTRAP) {
        switch (code) {
            case TRAP_BRKPT: return "TRAP_BRKPT";
            case TRAP_TRACE: return "TRAP_TRACE";
        }
    } else if (signo == SIGFPE) {
        switch (code) {
            case FPE_INTDIV: return "FPE_INTDIV";
            case FPE_INTOVF: return "FPE_INTOVF";
            case FPE_FLTDIV: return "FPE_FLTDIV";
            case FPE_FLTOVF: return "FPE_FLTOVF";
            case FPE_FLTUND: return "FPE_FLTUND";
            case FPE_FLTRES: return "FPE_FLTRES";
            case FPE_FLTINV: return "FPE_FLTINV";
            case FPE_FLTSUB: return "FPE_FLTSUB";
        }
    } else if (signo == SIGBUS) {
        switch (code) {
            case BUS_ADRALN: return "BUS_ADRALN";
            case BUS_ADRERR: return "BUS_ADRERR";
            case BUS_OBJERR: return "BUS_OBJERR";
        }
    } else if (signo == SIGILL) {
        switch (code) {
            case ILL_ILLOPC: return "ILL_ILLOPC";
            case ILL_ILLOPN: return "ILL_ILLOPN";
            case ILL_ILLADR: return "ILL_ILLADR";
            case ILL_ILLTRP: return "ILL_ILLTRP";
            case ILL_PRVOPC: return "ILL_PRVOPC";
            case ILL_PRVREG: return "ILL_PRVREG";
            case ILL_COPROC: return "ILL_COPROC";
            case ILL_BADSTK: return "ILL_BADSTK";
        }
    }
    return NULL;
}

void print_signal_stop(int stopsig, const siginfo_t *si) {
    fprintf(stderr, "--- ");
    print_signame(stopsig);
    fprintf(stderr, " {si_signo=");
    print_signame(si->si_signo);

    const char *cname = si_code_name(si->si_signo, si->si_code);
    if (cname) fprintf(stderr, ", si_code=%s", cname);
    else       fprintf(stderr, ", si_code=%d", si->si_code);

    /* User/tkill/queue sources carry sender pid+uid; kernel sources don't. */
    if (si->si_code == SI_USER || si->si_code == SI_TKILL
        || si->si_code == SI_QUEUE) {
        fprintf(stderr, ", si_pid=%d, si_uid=%d",
                (int)si->si_pid, (int)si->si_uid);
    }
    fprintf(stderr, "} ---\n");
}

void print_kill_trailer(int sig) {
    fprintf(stderr, "+++ killed by ");
    print_signame(sig);
    fprintf(stderr, " +++\n");
}
