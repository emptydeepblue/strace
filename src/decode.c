/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   decode.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: everonel <everonel@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/07 12:10:22 by everonel          #+#    #+#             */
/*   Updated: 2026/05/07 15:20:32 by everonel         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "ft_strace.h"
# include "syscalls.h"
# include <ctype.h>
# include <fcntl.h>      /* AT_FDCWD, O_*, AT_* */
# include <sys/mman.h>   /* PROT_*, MAP_* */
# include <unistd.h>     /* R_OK, W_OK, X_OK, F_OK */

#define MAX_STR_PRINT 32

typedef struct { unsigned long val; const char *name; } flag_t;

/* ------------------------------------------------------------------------- */
/*  Generic OR-bitmask decoder                                               */
/*                                                                           */
/*  Walks a sentinel-terminated table and emits NAME1|NAME2 for each flag    */
/*  whose bits are present in `v`. Leftover bits are appended as 0xN.        */
/*  When `v == 0` and `zero_name` is non-NULL, prints `zero_name` (e.g.      */
/*  "PROT_NONE", "F_OK"); otherwise an empty value prints "0".               */
/* ------------------------------------------------------------------------- */

static int print_flags(unsigned long v, const flag_t *tbl, const char *zero_name) {
    int n = 0, first = 1;
    unsigned long left = v;
    for (const flag_t *p = tbl; p->name; p++) {
        if (p->val && (left & p->val) == p->val) {
            n += fprintf(stderr, "%s%s", first ? "" : "|", p->name);
            first = 0;
            left &= ~p->val;
        }
    }
    if (first && left == 0)
        return fprintf(stderr, "%s", zero_name ? zero_name : "0");
    if (left)
        n += fprintf(stderr, "%s%#lx", first ? "" : "|", left);
    return n;
}

/* ------------------------------------------------------------------------- */
/*  open(2) / openat(2) flags                                                */
/*                                                                           */
/*  Low 2 bits are an enum (O_ACCMODE: O_RDONLY=0, O_WRONLY=1, O_RDWR=2),    */
/*  the rest are bit flags. Strace prints the access mode unconditionally,   */
/*  then OR-joins the rest.                                                  */
/* ------------------------------------------------------------------------- */

static const flag_t open_bit_tbl[] = {
    { O_CREAT,     "O_CREAT" },
    { O_EXCL,      "O_EXCL" },
    { O_NOCTTY,    "O_NOCTTY" },
    { O_TRUNC,     "O_TRUNC" },
    { O_APPEND,    "O_APPEND" },
    { O_NONBLOCK,  "O_NONBLOCK" },
    { O_DSYNC,     "O_DSYNC" },
    { O_DIRECTORY, "O_DIRECTORY" },
    { O_NOFOLLOW,  "O_NOFOLLOW" },
    { O_CLOEXEC,   "O_CLOEXEC" },
#ifdef O_PATH
    { O_PATH,      "O_PATH" },
#endif
#ifdef O_TMPFILE
    { O_TMPFILE,   "O_TMPFILE" },
#endif
    { 0, NULL }
};

static int print_open_flags(unsigned long v) {
    static const char *acc_names[] = { "O_RDONLY", "O_WRONLY", "O_RDWR" };
    unsigned long acc  = v & O_ACCMODE;
    unsigned long rest = v & ~(unsigned long)O_ACCMODE;
    int n = fprintf(stderr, "%s", acc < 3 ? acc_names[acc] : "0");
    if (rest) {
        n += fprintf(stderr, "|");
        n += print_flags(rest, open_bit_tbl, NULL);
    }
    return n;
}

/* ------------------------------------------------------------------------- */
/*  *at(2) flags — 4th arg of newfstatat, 3rd of statx, etc.                 */
/* ------------------------------------------------------------------------- */

static const flag_t at_flags_tbl[] = {
    { AT_SYMLINK_NOFOLLOW, "AT_SYMLINK_NOFOLLOW" },
    { AT_REMOVEDIR,        "AT_REMOVEDIR" },
    { AT_SYMLINK_FOLLOW,   "AT_SYMLINK_FOLLOW" },
#ifdef AT_NO_AUTOMOUNT
    { AT_NO_AUTOMOUNT,     "AT_NO_AUTOMOUNT" },
#endif
#ifdef AT_EMPTY_PATH
    { AT_EMPTY_PATH,       "AT_EMPTY_PATH" },
#endif
#ifdef AT_EACCESS
    { AT_EACCESS,          "AT_EACCESS" },
#endif
    { 0, NULL }
};

/* ------------------------------------------------------------------------- */
/*  mmap/mprotect prot — PROT_NONE is the all-zero name                      */
/* ------------------------------------------------------------------------- */

static const flag_t prot_tbl[] = {
    { PROT_READ,  "PROT_READ" },
    { PROT_WRITE, "PROT_WRITE" },
    { PROT_EXEC,  "PROT_EXEC" },
    { 0, NULL }
};

/* ------------------------------------------------------------------------- */
/*  mmap flags                                                               */
/* ------------------------------------------------------------------------- */

static const flag_t map_flags_tbl[] = {
    { MAP_SHARED,    "MAP_SHARED" },
    { MAP_PRIVATE,   "MAP_PRIVATE" },
    { MAP_FIXED,     "MAP_FIXED" },
    { MAP_ANONYMOUS, "MAP_ANONYMOUS" },
#ifdef MAP_DENYWRITE
    { MAP_DENYWRITE, "MAP_DENYWRITE" },
#endif
#ifdef MAP_STACK
    { MAP_STACK,     "MAP_STACK" },
#endif
#ifdef MAP_NORESERVE
    { MAP_NORESERVE, "MAP_NORESERVE" },
#endif
#ifdef MAP_POPULATE
    { MAP_POPULATE,  "MAP_POPULATE" },
#endif
    { 0, NULL }
};

/* ------------------------------------------------------------------------- */
/*  access(2) mode — F_OK is the all-zero name                               */
/* ------------------------------------------------------------------------- */

static const flag_t access_mode_tbl[] = {
    { R_OK, "R_OK" },
    { W_OK, "W_OK" },
    { X_OK, "X_OK" },
    { 0, NULL }
};

/* ------------------------------------------------------------------------- */
/*  String/buffer escape — read from tracee via process_vm_readv             */
/* ------------------------------------------------------------------------- */

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

static int print_str(pid_t pid, unsigned long val) {
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

static int print_buf(pid_t pid, unsigned long val, size_t want) {
    if (val == 0) return fprintf(stderr, "NULL");
    size_t cap = want > MAX_STR_PRINT ? MAX_STR_PRINT : want;
    char buf[MAX_STR_PRINT];
    struct iovec local  = { buf, cap };
    struct iovec remote = { (void *)val, cap };
    ssize_t n = process_vm_readv(pid, &local, 1, &remote, 1, 0);
    if (n < 0) return fprintf(stderr, "0x%lx", val);
    return print_quoted(buf, (size_t)n, want > (size_t)n);
}

/* ------------------------------------------------------------------------- */
/*  Dispatcher                                                               */
/* ------------------------------------------------------------------------- */

int format_arg(pid_t pid, arg_type_t type, unsigned long val,
               const unsigned long *args, int idx) {
    switch (type) {
    case ARG_NONE:        return fprintf(stderr, "?");
    case ARG_INT:         return fprintf(stderr, "%d",  (int)val);
    case ARG_LONG:        return fprintf(stderr, "%ld", (long)val);
    case ARG_SIZE:        return fprintf(stderr, "%zu", (size_t)val);
    case ARG_FD:          return fprintf(stderr, "%d",  (int)val);
    case ARG_DIRFD:       return ((int)val == AT_FDCWD)
                                 ? fprintf(stderr, "AT_FDCWD")
                                 : fprintf(stderr, "%d", (int)val);
    case ARG_MODE:        return fprintf(stderr, "%#o", (unsigned)val);
    case ARG_FLAGS:       return fprintf(stderr, "%#x", (unsigned)val);
    case ARG_SIGNUM:      return fprintf(stderr, "%d",  (int)val);
    case ARG_OPEN_FLAGS:  return print_open_flags(val);
    case ARG_AT_FLAGS:    return print_flags(val, at_flags_tbl, NULL);
    case ARG_PROT:        return print_flags(val, prot_tbl, "PROT_NONE");
    case ARG_MAP_FLAGS:   return print_flags(val, map_flags_tbl, NULL);
    case ARG_ACCESS_MODE: return print_flags(val, access_mode_tbl, "F_OK");
    case ARG_PTR:         return val ? fprintf(stderr, "0x%lx", val)
                                     : fprintf(stderr, "NULL");
    case ARG_STR:         return print_str(pid, val);
    case ARG_BUF:         return print_buf(pid, val, args[idx + 1]);
    default:              return val ? fprintf(stderr, "0x%lx", val)
                                     : fprintf(stderr, "NULL");
    }
}
