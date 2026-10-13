#!/usr/bin/env bash
set -eu

printf 'OS: '
lsb_release -ds
printf 'Kernel: '
uname -r
printf 'GCC: '
gcc -dumpfullversion
printf 'OpenSSL: '
openssl version
printf 'Open MPI: '
mpirun --version | head -n 1
printf 'Python: '
python3 --version
