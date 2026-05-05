/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   syscall_table.c                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: everonel <everonel@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/05 11:13:19 by everonel          #+#    #+#             */
/*   Updated: 2026/05/05 12:00:20 by everonel         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "syscalls.h"

const syscall_info_t syscalls_x86_64[] = {
# include "syscalls_x86_64.inc"
};
const size_t syscalls_x86_64_count = sizeof(syscalls_x86_64) / sizeof(syscalls_x86_64[0]);

const syscall_info_t syscalls_i386[] = {
# include "syscalls_i386.inc"
};
const size_t syscalls_i386_count = sizeof(syscalls_i386) / sizeof(syscalls_i386[0]);