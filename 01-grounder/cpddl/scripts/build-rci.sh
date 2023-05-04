#!/bin/bash
#SBATCH -p cpu # partition (queue)
#SBATCH -N 1 # number of nodes
#SBATCH -n 8 # number of cores
#SBATCH -x n33 # exclude node n33
#SBATCH --mem 1G # memory pool for all cores
#SBATCH -t 0-2:00 # time (D-HH:MM)
#SBATCH -o build.%N.%j.out # STDOUT
#SBATCH -e build.%N.%j.err # STDERR

# This script builds the project on the RCI cluster.
# Submit this task from the top directory of this repository as follows:
# sbatch scripts/build-rci.sh

set -x

NCPUS=8

cat >Makefile.local <<EOF
CFLAGS = -march=native
CPLEX_CFLAGS = -I/mnt/appl/software/CPLEX/12.9-foss-2018b/cplex/include
CPLEX_LDFLAGS = -L/mnt/appl/software/CPLEX/12.9-foss-2018b/cplex/bin/x86-64_linux -Wl,-rpath=/mnt/appl/software/CPLEX/12.9-foss-2018b/cplex/bin/x86-64_linux -lcplex1290
EOF


make mrproper
make -j$NCPUS boruvka
make -j$NCPUS opts
make -j$NCPUS bliss
make -j$NCPUS
make -j$NCPUS -C bin
