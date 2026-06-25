#ifndef SIGNALS_H
# define SIGNALS_H

# include <signal.h>

void print_signal_stop(int stopsig, const siginfo_t *si);
void print_kill_trailer(int sig);

#endif
