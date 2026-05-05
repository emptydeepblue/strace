# TODO — ft_strace

A milestone-driven plan. Each milestone produces a runnable, demoable binary; finish one before starting the next. The "Verify" line under each milestone is what you should be able to show a peer-evaluator.

---

## Milestone 0 — Background reading

Don't skip this. The subject lists exactly which `ptrace` requests are allowed and that list dictates the whole control flow. If you start coding before understanding why `PTRACE_SEIZE` was chosen over `PTRACE_TRACEME`, you'll write the wrong skeleton.

- [x] `man 2 ptrace` — read the **Stopped states** section in full (syscall-stop vs signal-delivery-stop vs group-stop vs PTRACE_EVENT-stop). This is the model the tracer loop has to implement.
- [x] `man 1 strace` — focus on the default output format, the `--- SIG... ---` lines, and the `+++ exited ... +++` trailer.
- [x] `man 2 process_vm_readv` — this replaces the disallowed `PTRACE_PEEKDATA` for reading tracee buffers.
- [x] Skim `arch/x86/entry/syscalls/syscall_64.tbl` and `syscall_32.tbl` in the kernel source (or `/usr/include/asm/unistd_64.h`, `unistd_32.h`) — these are the two tables you need to ship.
- [x] Run `strace -f /bin/ls 2>&1 | head -50` and `strace -o ref.txt /bin/ls; cat ref.txt` to see the exact output you're targeting.
- [x] Read x86_64 + i386 syscall calling conventions: which registers hold the syscall number, the args, the return value. Note that `orig_rax` / `orig_eax` (not `rax`/`eax`) hold the syscall number on entry.

**Verify:** you can answer, without looking it up: "what's the difference between a syscall-stop and a signal-delivery-stop, and how do you tell them apart?"

---

## Milestone 1 — Launcher + minimal trace loop

Goal: `./ft_strace /bin/ls` runs `ls` to completion under our tracer and prints **nothing** but the final `+++ exited with 0 +++`. No syscall decoding yet.

- [x] Argument parsing: reject zero args with a usage message to stderr; resolve `argv[1]` (mandatory: only handle absolute/relative paths — `$PATH` is bonus).
- [x] `fork()`. Child: `raise(SIGSTOP)` (or sync via pipe) so the parent can attach **before** `execve`, then `execvp`/`execve` the target.
- [x] Parent: `waitpid` for the child's `SIGSTOP`, then `PTRACE_SEIZE` with options `PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACEEXEC | PTRACE_O_TRACEEXIT`.
- [x] Use `PTRACE_INTERRUPT` if you need to deliver the initial stop yourself; otherwise let the child's `SIGSTOP` serve as the attach point.
- [x] Main loop: `waitpid(tracee, &status, 0)` → if `WIFEXITED`/`WIFSIGNALED`, print exit/kill trailer and break; otherwise classify the stop and `PTRACE_SYSCALL` to continue.
- [x] Print `+++ exited with N +++` on `WIFEXITED`, `+++ killed by SIGxxx +++` on `WIFSIGNALED`.

**Verify:** `./ft_strace /bin/ls` lists the directory and prints the exit trailer. No crashes when the target doesn't exist, when given no args, when interrupted by Ctrl-C.

---

## Milestone 2 — Syscall name printing (64-bit only)

Goal: every syscall the tracee makes is printed as `name(...) = retval`, with `...` as a placeholder. Still no real argument decoding.

- [x] Build a syscall table for x86_64: array indexed by syscall number, each entry `{ name, nargs, arg_types[6] }`. Generate it from `/usr/include/asm/unistd_64.h` with a small awk/sed script committed to the repo (don't hand-type 400 entries).
- [x] On every syscall-stop, call `PTRACE_GETREGSET(NT_PRSTATUS)` once, read `orig_rax`, look up the name.
- [x] Track entry vs exit per-tracee with a boolean toggle. On entry: print `name(` and arg placeholders; on exit: read `rax`, print `) = retval`.
- [x] Format negative `rax` in the errno range as `-1 ENAME (strerror)` like real strace.  NOTE(missing ENOTTY case for ioctl, but that's a syscall-specific quirk you can skip).
- [x] Handle the very first stop after `execve` correctly — the `execve` return is special (it appears as a syscall-exit-stop with a successful return even though the registers were clobbered).

**Verify:** `./ft_strace /bin/ls /tmp` produces output that looks structurally like real strace (one line per syscall, names match), diffed against `strace -o ref ls /tmp`.

---

## Milestone 3 — Argument decoding

Goal: arguments are formatted, not just `...`. Output is close enough to real strace that a reviewer skimming both side-by-side sees only minor formatting differences.

- [ ] Define an arg-type enum: `INT`, `LONG`, `FD`, `PTR`, `STR` (NUL-terminated), `BUF` (with length from another arg), `FLAGS` (bitmask), `MODE` (octal), `SIGNUM`, `STRUCT_*` (one per shape you care about: `stat`, `sockaddr`, `iovec`, ...).
- [ ] Per-type formatters. `STR`/`BUF` use `process_vm_readv` to copy from the tracee — clamp the length, escape non-printables, suffix with `...` if truncated.
- [ ] Flag/mode tables for the common ones: `O_*` (open), `PROT_*` (mmap), `MAP_*` (mmap), `AT_*` (openat dirfd: `AT_FDCWD`), `S_IF*` modes, `MS_*` (mount), `CLONE_*`. You don't need every flag — match what real strace shows for `ls`, `cat`, `echo`, `/bin/sh -c true`.
- [ ] Pretty-print structures you encounter often: `struct stat` (just the fields strace shows), `struct timespec`, `struct sockaddr_in`. Read them from the tracee with `process_vm_readv`.
- [ ] Pointer args that aren't dereferenced: print as `0x%lx` or `NULL`.

**Verify:** `diff <(./ft_strace /bin/ls 2>&1) <(strace /bin/ls 2>&1)` shows mostly aligned syscall lines. Spot-check a program that opens files: paths, flags, modes look right.

---

## Milestone 4 — Signal handling (the dangerous milestone)

This is the part the subject warns about twice. Do not skip the classification logic.

- [ ] On every stop, call `PTRACE_GETSIGINFO` and inspect `WSTOPSIG(status)` + `si_code` to classify:
  - **syscall-stop**: `WSTOPSIG == SIGTRAP | 0x80` (because we set `TRACESYSGOOD`).
  - **PTRACE_EVENT stop**: `status >> 16 != 0` (e.g. `PTRACE_EVENT_EXEC`).
  - **group-stop**: `WSTOPSIG ∈ {SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU}` AND `PTRACE_GETSIGINFO` returns `EINVAL`. Resume with `PTRACE_LISTEN`, **not** `PTRACE_SYSCALL`.
  - **signal-delivery-stop**: anything else. Print `--- SIGxxx {si_signo=..., si_code=..., ...} ---` and inject the signal back via `PTRACE_SYSCALL`'s data argument.
- [ ] Make sure a syscall interrupted by a signal prints correctly: real strace shows the partial entry line, then the signal line, then the exit line returning `? ERESTARTSYS` or similar. Mirror that.
- [ ] Final exit: distinguish `+++ exited with N +++` (normal) from `+++ killed by SIGxxx +++` (signaled). For `SIGKILL` specifically, you may not see a signal-delivery-stop at all — go straight to `WIFSIGNALED`.
- [ ] Don't crash on unexpected stops. If you can't classify a stop, log a warning to stderr and continue with `PTRACE_SYSCALL`.

**Verify:** `./ft_strace /bin/sleep 5` then Ctrl-C — you should see the `SIGINT` signal-delivery line and the `+++ killed by SIGINT +++` trailer. `./ft_strace bash -c 'kill -STOP $$; sleep 1'` exercises group-stop handling.

---

## Milestone 5 — 32-bit tracee support

Goal: `./ft_strace ./hello32` (where `hello32` is built with `gcc -m32`) works the same as a 64-bit binary.

- [ ] After `PTRACE_GETREGSET(NT_PRSTATUS)`, check `iov.iov_len`: `sizeof(struct user_regs_struct)` → x86_64, smaller (matches `struct i386_user_regs_struct` from `<sys/user.h>` / `<asm/ptrace.h>`) → i386.
- [ ] Second syscall table built from `unistd_32.h`. Same arg-type metadata format so the formatters can be shared.
- [ ] Re-detect the personality on every `PTRACE_EVENT_EXEC` — a 64-bit process can `execve` a 32-bit binary mid-trace.
- [ ] Different argument register layout for i386 (`ebx, ecx, edx, esi, edi, ebp`); add a per-personality "read arg N" helper.

**Verify:** Compile a small C program with `-m32`, run it under both real `strace` and `ft_strace`, confirm syscall names match. (You may need `gcc-multilib` installed.)

---

## Milestone 6 — Robustness pass

The grader is going to try to break this. Find the cracks first.

- [ ] Run under valgrind: `valgrind --error-exitcode=1 ./ft_strace /bin/ls`. No leaks, no invalid reads.
- [ ] Try every failure mode: nonexistent target, target without `+x`, target that's a directory, target that's a script with bad shebang, target that immediately calls `exit_group`, target that segfaults, target that `fork`s (single-process is fine for mandatory — just don't crash), target that `execve`s another binary mid-run.
- [ ] Send signals from outside: `kill -TERM $(pidof ft_strace)` while it's tracing — make sure the tracee is detached cleanly. Install a `SIGINT`/`SIGTERM` handler in the tracer that detaches and exits.
- [ ] Verify behavior with a setuid binary as target (likely fails with `EPERM` on `PTRACE_SEIZE` — error message must be sane, not a crash).
- [ ] No `Werror` warnings. No compiler warnings under `-Wall -Wextra -Wpedantic`.

**Verify:** a 30-minute "try to break it" session by you or a peer produces no segfault, double-free, or hang. Output for normal programs still matches Milestone 4's verification.

---

## Milestone 7 — Bonus (only if mandatory is perfect)

Subject is clear: **bonus is graded only if the mandatory part works without malfunction**. Don't start until Milestone 6 is green.

- [ ] `-c` option: instead of per-call output, accumulate per-syscall counts and total time, print a summary table at the end (`% time`, `seconds`, `usecs/call`, `calls`, `errors`, `syscall`). Time deltas come from clock_gettime around the wait/continue cycle.
- [ ] `$PATH` resolution: if `argv[1]` doesn't contain `/`, walk `getenv("PATH")` and try each component. Match `execvp`'s semantics for error messages.

---

## Cross-cutting concerns (apply throughout)

- **Single-process only for mandatory.** Don't set `PTRACE_O_TRACEFORK/CLONE/VFORK` — multi-process tracing isn't required and adds large complexity. Add it only if you see it specifically called out in your defense rubric.
- **One `Makefile` rule.** When you split files, append to `SOURCES` in the Makefile (it doesn't glob).
- **Commit per milestone, not per file.** Each commit message names the milestone and what works after it.
- **Keep `inc/ft_strace.h` lean.** Per-module headers (`inc/syscalls.h`, `inc/decode.h`, `inc/signals.h`) once the codebase grows past ~3 source files.
