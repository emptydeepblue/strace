/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   decode.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: everonel <everonel@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/07 12:10:22 by everonel          #+#    #+#             */
/*   Updated: 2026/05/07 15:17:45 by everonel         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "ft_strace.h"
# include "syscalls.h"
# include <ctype.h>
# include <fcntl.h>   /* AT_FDCWD */

#define MAX_STR_PRINT 32

static int print_quoted(const char *s, size_t len, int truncated) {
    int n = fprintf(stderr, "\"");
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\\' || c == '"') n += fprintf(stderr, "\\%c", c);
        else if (c == '\n')        n += fprintf(stderr, "\\n");
        else if (c == '\t')        n += fprintf(stderr, "\\t");
        else if (c == '\r')        n += fprintf(stderr, "\\r");
        else if (isprint(c))       n += fprintf(stderr, "%c", c);
        else                       n += fprintf(stderr, "\\x%02x", c);
    }
    n += fprintf(stderr, "\"%s", truncated ? "..." : "");
    return n;
}

int format_arg(pid_t pid, arg_type_t type, unsigned long val,
               const unsigned long *args, int idx) {
    switch (type) {
    case ARG_NONE:  return fprintf(stderr, "?");
    case ARG_INT:   return fprintf(stderr, "%d",  (int)val);
    case ARG_LONG:  return fprintf(stderr, "%ld", (long)val);
    case ARG_SIZE:  return fprintf(stderr, "%zu", (size_t)val);
    case ARG_FD:    return fprintf(stderr, "%d",  (int)val);
    case ARG_DIRFD:
        if ((int)val == AT_FDCWD) return fprintf(stderr, "AT_FDCWD");
        return fprintf(stderr, "%d", (int)val);
    case ARG_MODE:  return fprintf(stderr, "%#o", (unsigned)val);
    case ARG_FLAGS: return fprintf(stderr, "%#x", (unsigned)val);  /* TODO: 3.3 — symbolic names */
    case ARG_SIGNUM: return fprintf(stderr, "%d", (int)val);       /* TODO: 3.3 — SIGxxx names */
    case ARG_PTR:   return val ? fprintf(stderr, "0x%lx", val) : fprintf(stderr, "NULL");
    case ARG_STR: {
        if (val == 0) return fprintf(stderr, "NULL");
        char buf[MAX_STR_PRINT + 1];
        struct iovec local  = { buf, sizeof buf - 1 };
        struct iovec remote = { (void *)val, sizeof buf - 1 };
        ssize_t n = process_vm_readv(pid, &local, 1, &remote, 1, 0);
        if (n < 0) return fprintf(stderr, "0x%lx", val);
        size_t slen = strnlen(buf, (size_t)n);
        int truncated = (slen == (size_t)n);  /* hit the read cap, no NUL */
        return print_quoted(buf, slen, truncated);
    }
    case ARG_BUF: {
        if (val == 0) return fprintf(stderr, "NULL");
        size_t want = args[idx + 1];
        size_t cap = want > MAX_STR_PRINT ? MAX_STR_PRINT : want;
        char buf[MAX_STR_PRINT];
        struct iovec local  = { buf, cap };
        struct iovec remote = { (void *)val, cap };
        ssize_t n = process_vm_readv(pid, &local, 1, &remote, 1, 0);
        if (n < 0) return fprintf(stderr, "0x%lx", val);
        return print_quoted(buf, (size_t)n, want > (size_t)n);
    }
    default:
        return val ? fprintf(stderr, "0x%lx", val) : fprintf(stderr, "NULL");
    }
}
