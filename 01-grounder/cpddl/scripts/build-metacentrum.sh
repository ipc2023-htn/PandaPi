#!/bin/bash

# This script builds the project on the Czech National Grid.
# Submit this task from the top directory of this repository as follows:
# qsub -l walltime=8:0:0 -l select=1:ncpus=8:mem=64gb:scratch_local=30gb:cluster=zenon ./scripts/build-metacentrum.sh

set -x

HOME_ROOT=$PBS_O_WORKDIR
ROOT=$SCRATCHDIR/repo
NCPUS=$PBS_NCPUS

mkdir $ROOT
rsync -avc $HOME_ROOT/ $ROOT/

cd $ROOT
cat >Makefile.local <<EOF
CFLAGS = -march=native
EOF

make mrproper
make -j$NCPUS boruvka
make -j$NCPUS opts
make -j$NCPUS
make -j$NCPUS -C bin

rsync -avc $ROOT/bin/ $HOME_ROOT/bin/
rm -rf $SCRATCHDIR/*
