# ptrace from zero — a primer for ft_strace

This is a refresher for everything `ptrace_stopped_states.md` assumes you already know. If you've done Philosophers but haven't touched processes or signals seriously since, **read this first**, in order. Each section ends with a short "keep this" box — that's the part you actually need cached in your head before reading the stopped-states doc.

---

## 1. Processes, `fork`, `waitpid`

### The mental model

A **process** is a running instance of a program. It has:
- a PID (an integer name)
- its own memory space (heap, stack, code, data)
- its own file descriptors
- a **state** (running, sleeping in a syscall, stopped, zombie, ...)

The state is tracked by the kernel. You don't see it directly — you observe it through syscalls like `waitpid`.

### `fork`

`fork()` makes the current process create a near-perfect copy of itself. After `fork()` returns, **two processes exist** running the same code from the same line. The only thing that distinguishes them is what `fork` returned:

```c
pid_t pid = fork();
if (pid == 0) {
    // I am the child
} else if (pid > 0) {
    // I am the parent; pid is my child's PID
} else {
    // fork failed
}
```

Both processes have the same memory contents at this instant, but from now on they evolve independently — a write in the child doesn't affect the parent.

### `execve`

Often the child immediately replaces its program with a different one:

```c
execve("/bin/ls", argv, envp);
// nothing here ever runs unless execve failed
```

`execve` keeps the PID but throws away the memory, code, etc., and starts running the new program from its `_start`. Same process, different program.

The `fork` + `execve` combo is how every command in your shell is launched. **ft_strace does exactly this**: fork a child, have the child `execve` the target program, and the parent traces it.

### `waitpid`

The parent learns about state changes in its child by calling `waitpid`:

```c
int status;
pid_t who = waitpid(pid, &status, options);
// blocks until the child's state changes; status encodes what changed
```

`status` is an opaque integer. You decode it with macros:

| macro | meaning |
| --- | --- |
| `WIFEXITED(status)` | child called `exit()` normally |
| `WEXITSTATUS(status)` | (if the above) the exit code 0–255 |
| `WIFSIGNALED(status)` | child was killed by a signal |
| `WTERMSIG(status)` | (if the above) which signal killed it |
| `WIFSTOPPED(status)` | child is **stopped** (not dead, not running) |
| `WSTOPSIG(status)` | (if the above) the "signal" that stopped it |

In a normal shell program, `waitpid` returns once: when the child dies. With ptrace, the child gets stopped many times before it dies, and `waitpid` returns once **per stop** — that's why the ft_strace main loop is a `while (waitpid(...))` loop.

> **Keep this for ft_strace:**
> - You'll `fork`, the child will `execve` the target, the parent will `waitpid` in a loop.
> - Every iteration: decode `status` to know if the child stopped (`WIFSTOPPED` — keep going) or terminated (`WIFEXITED` / `WIFSIGNALED` — print trailer and exit).
> - `WSTOPSIG(status)` tells you the "signal" that caused the stop, but **with ptrace, that "signal" is often synthetic** (e.g. `SIGTRAP | 0x80` means "syscall-stop", not a real `SIGTRAP`). That's the whole game in section 5.

---

## 2. Signals

### What they are

A **signal** is a one-bit, asynchronous notification from the kernel to a process. There are about 30 of them, each with a number and a name (`SIGINT` = 2, `SIGSEGV` = 11, ...). `man 7 signal` lists them all.

Signals are how the kernel says things like:
- "you tried to dereference an invalid pointer" → `SIGSEGV`
- "the user pressed Ctrl-C in the controlling terminal" → `SIGINT`
- "your child process terminated" → `SIGCHLD` (sent to the parent)
- "another process called `kill(your_pid, SIGTERM)`" → `SIGTERM`

### Default actions

Every signal has a **default action** the kernel takes if the process hasn't installed a handler:

| default | examples |
| --- | --- |
| **terminate** | `SIGTERM`, `SIGINT`, `SIGHUP`, `SIGPIPE` |
| **terminate + core dump** | `SIGSEGV`, `SIGABRT`, `SIGBUS`, `SIGFPE` |
| **ignore** | `SIGCHLD`, `SIGURG`, `SIGWINCH` |
| **stop the process** | `SIGSTOP`, `SIGTSTP`, `SIGTTIN`, `SIGTTOU` |
| **continue a stopped process** | `SIGCONT` |

A process can install a handler for almost any signal via `sigaction(2)`. Two signals are special: **`SIGKILL`** and **`SIGSTOP`** cannot be caught, blocked, or ignored. `SIGKILL` always kills; `SIGSTOP` always stops.

### `siginfo_t`

When a signal is delivered, the kernel attaches a `siginfo_t` struct with metadata: who sent it, why, what address triggered it (for `SIGSEGV`), the exit status of the dead child (for `SIGCHLD`), etc. Layout in `man 2 sigaction`. The fields ft_strace prints are `si_signo`, `si_code`, `si_pid`, sometimes `si_addr`.

### Why this matters for ptrace

When a tracee receives a signal, **the kernel doesn't deliver it immediately**. It pauses the tracee in a "signal-delivery-stop" and notifies the tracer first. The tracer then decides:
- "deliver it normally" (resume with the signal injected — default action runs, or the tracee's handler runs)
- "swallow it" (resume with no signal — the tracee never sees it)

This is why strace can show you `--- SIGSEGV ---` *before* the program crashes: the kernel paused the tracee at the moment of delivery, strace got first look.

> **Keep this for ft_strace:**
> - Signals are kernel-to-process events with default actions.
> - `siginfo_t` is the metadata blob — you fetch it with `PTRACE_GETSIGINFO`.
> - Inside ptrace, signals don't just "happen" to the tracee; the kernel pauses the tracee and asks the tracer first.
> - `SIGKILL` is the exception — the tracee just dies, no stop.

---

## 3. Syscalls

### What they are

Your code can't write directly to a file, allocate memory from the OS, or even exit. All of that requires the kernel. A **syscall** is the controlled doorway from user mode to kernel mode.

When you call `write(fd, buf, n)` in C, libc does not write to the file. It puts a syscall number and three arguments in registers, executes a special CPU instruction (`syscall` on x86_64), and the CPU switches to kernel mode. The kernel runs the actual write code, puts the return value in a register, and switches back.

### x86_64 syscall ABI

To make a syscall on x86_64, userspace does:

| register | role |
| --- | --- |
| `rax` | syscall number (e.g. `1` for `write`) |
| `rdi` | arg 1 |
| `rsi` | arg 2 |
| `rdx` | arg 3 |
| `r10` | arg 4 |
| `r8`  | arg 5 |
| `r9`  | arg 6 |

Then it executes `syscall`. On return, `rax` holds the result (negative values in the `-1..-4095` range are encoded errno values).

i386 is different: number in `eax`, args in `ebx, ecx, edx, esi, edi, ebp`, instruction is `int $0x80` (or `sysenter`). This is why ft_strace needs two syscall tables — same kernel, two ABIs.

### The "orig" trick

The kernel preserves the syscall number in a different register (`orig_rax` on x86_64) because `rax` is also the return-value register. Without `orig_rax`, by the time the syscall returns you'd have lost the number.

This is why `ptrace_stopped_states.md` says: "use `orig_rax` (entry) for the number; `rax` (exit) for the return value."

### Entry-stop vs exit-stop

ptrace can pause the tracee at **two** points around each syscall:
- **entry-stop**: just *before* the kernel runs the syscall. Registers hold the arguments the userspace prepared. Return value (`rax`) is meaningless — the syscall hasn't run.
- **exit-stop**: just *after* the kernel finishes the syscall. `rax` holds the return value. The arguments may or may not still be in their registers (some syscalls clobber them).

So one syscall produces two stops in ft_strace's main loop. You read args at the entry-stop, return value at the exit-stop, and join them into one line of output: `open("/etc/passwd", O_RDONLY) = 3`.

> **Keep this for ft_strace:**
> - Every syscall is a register-level transition; arguments and return value live in known registers (different on x86_64 vs i386).
> - You read those registers via `PTRACE_GETREGSET(NT_PRSTATUS)`.
> - Each syscall causes **two** ptrace stops: entry (read args) and exit (read return value). You toggle a per-tracee boolean to know which one you're in.

---

## 4. Job control (the short version)

### What it is

Job control is the shell feature where Ctrl-Z suspends the foreground process. Mechanically:
- The terminal sends `SIGTSTP` to the foreground process group when you press Ctrl-Z.
- `SIGTSTP`, `SIGSTOP`, `SIGTTIN`, `SIGTTOU` are the four "stop signals". Their default action is to put the process into the **stopped** state.
- A stopped process is alive but not scheduled. `ps` shows it as `T` (or `T+`).
- `SIGCONT` un-stops it.

You've used this even if you didn't know it: `Ctrl-Z` then `fg` is the user-visible cycle.

### Why ptrace cares

A traced process can also receive `SIGTSTP`/`SIGSTOP`. The kernel still puts it through the job-control "stopped" state, **on top of** the ptrace machinery. So the tracee is now stopped *for two reasons*: ptrace is holding it, and job control is holding it. If the tracer issues the normal `PTRACE_SYSCALL` to resume, the job-control hold is still there and the tracee won't actually run.

The right move is `PTRACE_LISTEN`: "I acknowledge this is a group-stop; let me know when the job-control state changes (e.g. `SIGCONT` arrives)." This is why `PTRACE_LISTEN` is on the subject's restricted list — it exists exactly for this case.

> **Keep this for ft_strace:**
> - Group-stop = the tracee got `SIGSTOP`/`SIGTSTP`/`SIGTTIN`/`SIGTTOU`, and the kernel put it into the job-control "stopped" state.
> - Resume with `PTRACE_LISTEN`, **not** `PTRACE_SYSCALL`, otherwise it just sits there.

---

## 5. ptrace itself

Now everything is in place. Here is the whole mechanism in one paragraph:

> `ptrace` lets one process (the **tracer**) attach to another (the **tracee**). The tracee runs normally **until** it hits an event the tracer subscribed to — entering a syscall, receiving a signal, calling `execve`, etc. When that happens, the **kernel freezes the tracee** and notifies the tracer by making the tracer's `waitpid` call return with `WIFSTOPPED(status)` true. The tracee stays frozen until the tracer **explicitly** issues a `ptrace` call (`PTRACE_SYSCALL`, `PTRACE_LISTEN`, ...) to resume it. That's the loop.

Concretely, ft_strace's main loop is:

```c
while (1) {
    pid_t w = waitpid(child, &status, __WALL);

    if (WIFEXITED(status))   { print_exit(status);   break; }
    if (WIFSIGNALED(status)) { print_killed(status); break; }
    if (!WIFSTOPPED(status)) continue; // shouldn't happen, but be safe

    // The kernel paused the tracee. Why?
    enum stop_kind k = classify(status, child);

    switch (k) {
    case SYSCALL_STOP:    handle_syscall(child); ptrace(PTRACE_SYSCALL, child, 0, 0); break;
    case SIGNAL_STOP:     print_signal(child);   ptrace(PTRACE_SYSCALL, child, 0, WSTOPSIG(status)); break;
    case GROUP_STOP:                              ptrace(PTRACE_LISTEN,  child, 0, 0); break;
    case EVENT_STOP:      handle_event(status);  ptrace(PTRACE_SYSCALL, child, 0, 0); break;
    }
}
```

That's the whole skeleton. The four `case` arms are exactly the four stop types from `ptrace_stopped_states.md`. The `classify` function is the decision tree in section 3 of that doc. The "`handle_syscall`" reads registers (section 3 of this primer) and prints the line. The "`print_signal`" reads `siginfo_t` (section 2) and prints the `--- SIGxxx ---` line.

> **Keep this for ft_strace:**
> - The main loop is `waitpid` → classify the stop → react → resume with the right ptrace call.
> - Four kinds of stop, each with its own resume call. That's all `ptrace_stopped_states.md` is teaching.

---

## What to read next

You should now be able to re-read `ptrace_stopped_states.md` and have it actually click. If something there still feels opaque, re-read the matching section here:

| stuck on... | re-read primer section |
| --- | --- |
| "the tracer", "the tracee", "waitpid status" | 1 |
| "signal-delivery-stop", "siginfo", "inject the signal" | 2 |
| "syscall-stop", "entry vs exit", "registers" | 3 |
| "group-stop", "PTRACE_LISTEN" | 4 |
| "the loop", "classify", "resume" | 5 |
