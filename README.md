# ft_strace

A from-scratch reimplementation of Linux `strace` in C: traces every system call of a program using `ptrace`, decodes arguments, flags and structures, and reports signals and exit status.

**Specialization:** Unix/Kernel
**XP:** 9,450 · **People:** 1 · **Status:** 🟡 In progress (see [Roadmap](#-roadmap))

---

## 🚀 Features

- Attaches to the tracee **before `execve`** using `PTRACE_SEIZE` + `PTRACE_INTERRUPT` (no `PTRACE_TRACEME`)
- Reads registers with `PTRACE_GETREGSET` and tracee memory with `process_vm_readv` (no `PTRACE_PEEKDATA`)
- Syscall-entry / syscall-exit tracking with `PTRACE_O_TRACESYSGOOD`
- Argument decoding: strings and buffers (escaped, truncated like strace), file descriptors, `AT_FDCWD`, octal modes, pointers / `NULL`
- Flag decoding: `O_*`, `PROT_*`, `MAP_*`, `AT_*`, access modes
- Struct pretty-printing: `struct stat`, `struct timespec`
- Return values formatted per syscall (hex for `mmap`/`brk`, `-1 ENOENT (No such file or directory)` for errors)
- Signal reporting (`--- SIGxxx {...} ---`) and exit trailers (`+++ exited with N +++` / `+++ killed by SIGxxx +++`)
- Syscall tables for x86_64 and i386 generated from kernel headers by a script ([`script/gen_syscalls.sh`](script/gen_syscalls.sh))

## 🛠️ Build & Run

**Prerequisites:** GCC, GNU Make, Linux x86_64

```bash
make                         # Build ./ft_strace
./ft_strace /bin/ls          # Trace a program
./ft_strace /bin/echo hello  # Arguments are passed to the tracee
```

**Sample output:**
```
brk(NULL) = 0x5570e0612000
mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7f9b3e222000
access("/etc/ld.so.preload", R_OK) = -1 ENOENT (No such file or directory)
openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY|O_CLOEXEC, 0) = 3
fstat(3, {st_mode=S_IFREG|0644, st_size=54704, ...}) = 0
close(3) = 0
...
exit_group(0) = ?
+++ exited with 0 +++
```

## 📁 Project Structure

```
strace/
├── src/
│   ├── main.c            # Argument parsing, fork + attach
│   ├── tracer.c          # Main waitpid loop, stop classification
│   ├── decode.c          # Argument / flag decoding
│   ├── decode_struct.c   # struct stat, struct timespec
│   ├── printer.c         # strace-style output, return values, errno
│   ├── signals.c         # Signal-delivery stops and signal names
│   ├── syscall_table.c   # Syscall metadata (name, arg types, return type)
│   └── syscalls_*.inc    # Generated x86_64 / i386 tables
├── inc/                  # Headers
├── script/               # Syscall table generator
├── docs/                 # ptrace notes and project roadmap
└── Makefile
```

## 🔧 Implementation

**Tracer loop:** `fork` → the child stops itself → the parent attaches with `PTRACE_SEIZE` (options `TRACESYSGOOD | TRACEEXEC | TRACEEXIT`) → loop on `waitpid`, classify each stop (syscall-stop, signal-delivery-stop, group-stop, event-stop), resume with `PTRACE_SYSCALL` or `PTRACE_LISTEN` for group-stops.

**Why `process_vm_readv`:** the subject forbids `PTRACE_PEEKDATA`, so string and struct arguments are copied from the tracee's address space in a single call, with length clamping and escaping of non-printable bytes.

My notes on the parts that are easy to get wrong are in [`docs/ptrace_primer.md`](docs/ptrace_primer.md) and [`docs/ptrace_stopped_states.md`](docs/ptrace_stopped_states.md).

## 🗺️ Roadmap

- [x] Launcher, trace loop, exit trailers
- [x] Syscall names and return values (x86_64)
- [x] Argument, flag and struct decoding
- [ ] Signal handling edge cases (interrupted syscalls, `ERESTARTSYS`, group-stops)
- [ ] 32-bit (i386) tracees: personality detection via the regset size
- [ ] Robustness pass: valgrind, setuid targets, tracer killed mid-trace
- [ ] Bonus: `-c` summary and `$PATH` resolution

Detailed plan: [`docs/TODO.md`](docs/TODO.md)

## 📚 Learning Objectives

- The `ptrace` stop model and how a debugger or tracer actually controls a process
- The x86_64 and i386 syscall ABIs (`orig_rax`, argument registers)
- Reading another process's memory safely
- How the kernel delivers signals to a traced process

## 🔗 References

- `man 2 ptrace`, `man 2 process_vm_readv`, `man 1 strace`
- Linux kernel syscall tables: `arch/x86/entry/syscalls/syscall_64.tbl`, `syscall_32.tbl`

---

*42 School — RNCP7 · Unix/Kernel*