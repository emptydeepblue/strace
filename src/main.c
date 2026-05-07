/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: everonel <everonel@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/28 14:51:41 by everonel          #+#    #+#             */
/*   Updated: 2026/05/07 11:49:23 by everonel         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "ft_strace.h"

int main(int ac, char **av) {
    if (ac < 2) {
        fprintf(stderr, "Usage: %s <program> [args...]\n", av[0]);
        return 1;
    }

    pid_t tracee = fork();
    if (tracee < 0) { perror("fork"); return 1; }
    
    if (tracee == 0) {
        raise(SIGSTOP);
        execvp(av[1], &av[1]);
        perror("execvp");
        _exit(127);
    }
    
    return run_tracer(tracee) < 0 ? 1 : 0;
}
