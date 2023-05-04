#!/bin/bash

make mrproper
make boruvka
make opts
make
make -C bin
