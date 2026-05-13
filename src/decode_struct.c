/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   decode_struct.c                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: everonel <everonel@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/07 16:00:00 by everonel          #+#    #+#             */
/*                                                                            */
/* ************************************************************************** */

# include "ft_strace.h"
# include "syscalls.h"
# include <sys/stat.h>
# include <time.h>

/* Read sizeof(*dst) bytes from tracee at `ptr` into `dst`. Returns 0 on full
   read, -1 otherwise. */
static int read_struct(pid_t pid, unsigned long ptr, void *dst, size_t sz) {
    struct iovec local  = { dst, sz };
    struct iovec remote = { (void *)ptr, sz };
    ssize_t n = process_vm_readv(pid, &local, 1, &remote, 1, 0);
    return (n == (ssize_t)sz) ? 0 : -1;
}

/* st_mode = S_IFTYPE | perms. The S_IF* values are an enum under S_IFMT,
   not a bitmask, so we look the type up and OR the octal perms next to it. */
static int print_st_mode(mode_t m) {
    static const struct { mode_t v; const char *n; } iftbl[] = {
        { S_IFREG,  "S_IFREG"  },
        { S_IFDIR,  "S_IFDIR"  },
        { S_IFCHR,  "S_IFCHR"  },
        { S_IFBLK,  "S_IFBLK"  },
        { S_IFIFO,  "S_IFIFO"  },
        { S_IFLNK,  "S_IFLNK"  },
        { S_IFSOCK, "S_IFSOCK" },
    };
    const char *type_name = NULL;
    for (size_t i = 0; i < sizeof iftbl / sizeof *iftbl; i++) {
        if ((m & S_IFMT) == iftbl[i].v) { type_name = iftbl[i].n; break; }
    }
    if (type_name) return fprintf(stderr, "%s|%#o", type_name, m & 07777);
    return fprintf(stderr, "%#o", m);
}

int print_stat(pid_t pid, unsigned long ptr, long long retval) {
    if (ptr == 0) return fprintf(stderr, "NULL");
    /* On error the kernel didn't fill the buffer — show the raw pointer like
       real strace does. */
    if (retval < 0) return fprintf(stderr, "0x%lx", ptr);

    struct stat st;
    if (read_struct(pid, ptr, &st, sizeof st) < 0)
        return fprintf(stderr, "0x%lx", ptr);

    int n = fprintf(stderr, "{st_mode=");
    n += print_st_mode(st.st_mode);
    n += fprintf(stderr, ", st_size=%lld, ...}", (long long)st.st_size);
    return n;
}

int print_timespec(pid_t pid, unsigned long ptr) {
    if (ptr == 0) return fprintf(stderr, "NULL");
    struct timespec ts;
    if (read_struct(pid, ptr, &ts, sizeof ts) < 0)
        return fprintf(stderr, "0x%lx", ptr);
    return fprintf(stderr, "{tv_sec=%lld, tv_nsec=%lld}",
                   (long long)ts.tv_sec, (long long)ts.tv_nsec);
}
