/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: everonel <everonel@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/28 14:51:41 by everonel          #+#    #+#             */
/*   Updated: 2026/05/05 09:27:42 by everonel         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "ft_strace.h"

int main (int ac, char **av) {
    
    if (ac < 2) {
        fprintf(stderr, "Usage: %s <program> [args...]\n", av[0]);
        return 1;
    }

    char *prog = av[1];
    char **args = &av[1];

    if (strace(prog, args) == -1) {
        fprintf(stderr, "Error: strace failed\n");
        return 1;
    }
    return 0;
}