#!/bin/bash
set -euo pipefail

awk '/^#define __NR_/ { printf "    [%s] = { .name = \"%s\" },\n", $3, substr($2, 6) }' \
  /usr/include/x86_64-linux-gnu/asm/unistd_64.h > src/syscalls_x86_64.inc

awk '/^#define __NR_/ { printf "    [%s] = { .name = \"%s\" },\n", $3, substr($2, 6) }' \
  /usr/include/x86_64-linux-gnu/asm/unistd_32.h > src/syscalls_i386.inc
