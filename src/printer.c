/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   printer.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: everonel <everonel@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/07 11:40:58 by everonel          #+#    #+#             */
/*   Updated: 2026/05/07 11:42:02 by everonel         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "ft_strace.h"

void print_syscall_retval(long long retval) {
    if ((unsigned long long)retval > -4096ULL) {
        int err = -retval;
        const char *n = strerrorname_np(err);
        printf(" = -1 %s (%s)\n", n ? n : "?", strerror(err));
    } else {
        printf(" = %lld\n", retval);
    }
}