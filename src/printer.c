/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   printer.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: everonel <everonel@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/07 11:40:58 by everonel          #+#    #+#             */
/*   Updated: 2026/05/13 10:23:12 by everonel         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "ft_strace.h"

void print_syscall_retval(arg_type_t ret_type, long long retval) {
    if ((unsigned long long)retval > -4096ULL) {
        int err = -retval;
        const char *n = strerrorname_np(err);
        fprintf(stderr, " = -1 %s (%s)\n", n ? n : "?", strerror(err));
    }
    if (ret_type == ARG_PTR) {
        fprintf(stderr, " = %#llx\n", (unsigned long long)retval);
    } else {
        fprintf(stderr, " = %lld\n", retval);
    }
}