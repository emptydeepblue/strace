# What to learn from `man 2 ptrace` — "Stopped states"

This is the single most important section of the man page for `ft_strace`. The tracer is, fundamentally, a loop of:

```
waitpid → classify the stop → react → resume the tracee
```

If you misclassify a stop, you either crash, hang, or print garbage. So the goal of reading this section is to be able to answer, on demand:

> "When `waitpid` returns with `WIFSTOPPED(status) == true`, **what kind of stop is it, and how do I resume from it?**"

There are exactly **four kinds of stop** the kernel can put the tracee into. Learn them as a closed set.

---

## 1. The mental model: what is a "ptrace-stop"?

A traced process is in a "ptrace-stop" when the kernel has frozen it and is waiting for the tracer to make a `ptrace(2)` call (typically `PTRACE_SYSCALL`, `PTRACE_CONT`, or `PTRACE_LISTEN`) to release it. Until then, the tracee does **nothing** — not even respond to `SIGKILL` in some cases (group-stop is the exception).

The tracer learns about a new stop only via `waitpid`. Every `waitpid` return on the tracee delivers a `status` word that encodes:

- **What stopped it** — `WSTOPSIG(status)` gives the "signal" (real signal, or a synthetic one like `SIGTRAP | 0x80`).
- **Whether it's a special event** — the high 16 bits of `status` (`status >> 16`) hold a `PTRACE_EVENT_*` code if this stop was caused by a ptrace event.

`WSTOPSIG` alone is **not enough** to classify the stop. You also need:

- `PTRACE_GETSIGINFO` — fills a `siginfo_t`; its `si_code` field tells you whether the "signal" is a real one being delivered or a synthetic stop reason. It also returns `EINVAL` for group-stops, which is itself the signal you've hit one.
- The high bits of `status` for `PTRACE_EVENT_*` stops.
- A bit of state you keep yourself (entry-vs-exit toggle for syscall-stops).

---

## 2. The four stop types

### a) Syscall-stop

**What it is.** The tracee is about to enter a syscall (entry-stop), or has just returned from one (exit-stop). Each syscall produces *two* of these: one before, one after.

**How to opt in.** Set `PTRACE_O_TRACESYSGOOD` via `PTRACE_SETOPTIONS`. Without this option, syscall-stops are reported as plain `SIGTRAP`, which is indistinguishable from a real `SIGTRAP` being delivered. **Always set this flag.**

**How to detect.** With `TRACESYSGOOD` set: `WSTOPSIG(status) == (SIGTRAP | 0x80)`, i.e. `0x85`.

**Entry vs exit.** The kernel doesn't tell you. **You have to track it yourself** with a per-tracee boolean that flips on every syscall-stop. (After `execve`, the next syscall-stop is an exit-stop for the `execve` itself — handle that edge case.)

**How to resume.** `PTRACE_SYSCALL` with `data == 0`. The tracee will run until the next syscall boundary.

**Where the data is.** `PTRACE_GETREGSET(NT_PRSTATUS)` — at entry, registers hold the syscall number (`orig_rax`/`orig_eax`) and arguments; at exit, `rax`/`eax` holds the return value.

### b) Signal-delivery-stop

**What it is.** The tracee was about to receive a real signal (e.g. `SIGSEGV`, `SIGINT`, a `SIGTRAP` that isn't a syscall-stop, etc.) and the kernel paused it so the tracer can decide what to do.

**How to detect.** `WSTOPSIG(status)` is some signal number that **isn't** `SIGTRAP|0x80`, **isn't** one of the four group-stop signals from a group-stop context (see below), and `status >> 16 == 0` (no PTRACE_EVENT). Confirm with `PTRACE_GETSIGINFO` — it will succeed and return a populated `siginfo_t`.

**How to resume.** `PTRACE_SYSCALL` (or `PTRACE_CONT`) with `data == WSTOPSIG(status)` — this **injects the signal** so the tracee actually receives it. If you pass `data == 0`, you've **suppressed** the signal. Real strace injects by default; only suppresses if `--inject` is used.

**What to print.** Real strace's `--- SIGxxx {si_signo=..., si_code=..., si_pid=..., ...} ---` line. The fields come from the `siginfo_t` you got via `GETSIGINFO`.

### c) Group-stop

**What it is.** Job-control stop — the tracee received `SIGSTOP`, `SIGTSTP`, `SIGTTIN`, or `SIGTTOU`, and would normally enter the "stopped" job-control state visible to a shell. Even under ptrace, the kernel still puts it through this state.

**Why it's special.** Unlike the other stops, the tracee in a group-stop is **also** in the kernel's job-control "stopped" state — it stays stopped even if you `PTRACE_CONT` it (sort of). The right way to handle it is `PTRACE_LISTEN`, which leaves the tracee in the group-stop state but tells the kernel to notify you when it changes (e.g. when `SIGCONT` arrives).

**How to detect.** `WSTOPSIG(status)` is one of `SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU` **and** `PTRACE_GETSIGINFO` fails with `EINVAL`. The `EINVAL` is the unambiguous signal that you're in a group-stop, not a signal-delivery-stop carrying the same signal number.

**How to resume.** `PTRACE_LISTEN` — *not* `PTRACE_SYSCALL`, *not* `PTRACE_CONT`. (`PTRACE_LISTEN` is one of the seven options the subject restricts you to — this is why it's on the list.) After `PTRACE_LISTEN`, the next `waitpid` will fire when the tracee leaves the group-stop (e.g. on `SIGCONT`).

**Common bug.** If you `PTRACE_SYSCALL` out of a group-stop, the tracee may not actually run, and you'll either hang or get confusing later stops. The `EINVAL` from `GETSIGINFO` is your only reliable detector.

### d) PTRACE_EVENT stop

**What it is.** A ptrace-specific event the kernel is notifying you about: `PTRACE_EVENT_EXEC`, `PTRACE_EVENT_EXIT`, `PTRACE_EVENT_FORK`, `PTRACE_EVENT_CLONE`, `PTRACE_EVENT_VFORK`, `PTRACE_EVENT_SECCOMP`, `PTRACE_EVENT_STOP` (used after `PTRACE_SEIZE` / `PTRACE_INTERRUPT`).

**How to opt in.** `PTRACE_O_TRACEEXEC`, `PTRACE_O_TRACEEXIT`, etc. via `PTRACE_SETOPTIONS`. For mandatory `ft_strace`, you only need `TRACEEXEC` (to re-detect 32/64-bit on `execve`) and arguably `TRACEEXIT`.

**How to detect.** `WSTOPSIG(status) == SIGTRAP` **and** `status >> 16 != 0`. The high 16 bits hold the `PTRACE_EVENT_*` code: `(status >> 16) & 0xffff == PTRACE_EVENT_EXEC`, etc.

**How to resume.** `PTRACE_SYSCALL` with `data == 0`. Do **not** inject `SIGTRAP` — it's synthetic.

**Note on `PTRACE_EVENT_STOP`.** This one is delivered after `PTRACE_SEIZE` + `PTRACE_INTERRUPT`, and also when a tracee enters or leaves a group-stop. You'll see it during attach.

---

## 3. The classification decision tree

After `waitpid` returns and you've confirmed `WIFSTOPPED(status)`:

```
1. Is status >> 16 != 0?
   → PTRACE_EVENT stop. Resume with PTRACE_SYSCALL, data=0.

2. Is WSTOPSIG(status) == (SIGTRAP | 0x80)?
   → Syscall-stop. Toggle entry/exit; decode regs; resume with PTRACE_SYSCALL, data=0.

3. Is WSTOPSIG(status) ∈ {SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU}
   AND PTRACE_GETSIGINFO fails with EINVAL?
   → Group-stop. Resume with PTRACE_LISTEN.

4. Otherwise:
   → Signal-delivery-stop. Print the --- SIGxxx --- line;
     resume with PTRACE_SYSCALL, data=WSTOPSIG(status) to inject.
```

The order matters. Check `PTRACE_EVENT` first (it can ride on top of `SIGTRAP`), then syscall-stop, then group-stop, then default to signal-delivery-stop.

---

## 4. The exit cases (not stops, but you'll see them)

`waitpid` can return with the tracee **not stopped** — it terminated:

- `WIFEXITED(status)` → normal exit. Print `+++ exited with WEXITSTATUS(status) +++`.
- `WIFSIGNALED(status)` → killed by signal. Print `+++ killed by SIG... +++`. For `SIGKILL` specifically you may go straight from running to this with no signal-delivery-stop in between.

When you see either, the tracee is gone — break out of the loop. Don't call `ptrace` on it again.

---

## 5. Subtleties worth memorizing

- **`SIGKILL` is special.** The kernel doesn't generate a signal-delivery-stop for it; the tracee just dies. Don't expect to see a `--- SIGKILL ---` line.
- **Restartable syscalls.** A signal interrupting a blocking syscall makes that syscall return `-ERESTARTSYS` (or similar) at the syscall-exit-stop, then the signal-delivery-stop fires. Real strace prints both: the partial syscall line ending in `?`, then the signal line, then (if the syscall is auto-restarted) the same syscall again. You don't have to be byte-exact, but don't lose the signal line.
- **`execve` is the weirdest syscall.** Its entry-stop and exit-stop straddle a complete address-space replacement. The exit-stop registers belong to the *new* program, not the caller. With `PTRACE_O_TRACEEXEC`, you also get a `PTRACE_EVENT_EXEC` stop right around it. Test this case explicitly.
- **The very first stop after attach.** With `PTRACE_SEIZE`, the tracee isn't stopped until you `PTRACE_INTERRUPT` it (or it stops itself). The first stop you see will be a `PTRACE_EVENT_STOP`, not a syscall-stop. Skip past it cleanly.
- **Re-using `siginfo`.** `PTRACE_GETSIGINFO` failing with `EINVAL` is *expected* for group-stops — don't treat it as a fatal error. Treat it as a classification signal.

---

## 6. How to test your understanding before writing code

Run real strace on these and predict, line-by-line, which stop type produced each output line. Then run them and check.

```sh
# only syscall-stops + exit
strace /bin/true

# adds a signal-delivery-stop for SIGINT, then killed-by trailer
strace sleep 5    # then Ctrl-C

# adds a group-stop (SIGTSTP), then SIGCONT
strace bash -c 'kill -STOP $$; sleep 1'

# adds PTRACE_EVENT_EXEC and the post-execve weirdness
strace bash -c 'exec /bin/true'

# adds signal-delivery-stop for SIGSEGV, then killed-by SIGSEGV
strace bash -c 'kill -SEGV $$'
```

If you can predict the sequence of stop types for each of those before running them, you've understood the section well enough to start on Milestone 1.
