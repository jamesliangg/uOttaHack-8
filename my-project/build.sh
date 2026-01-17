#!/bin/bash
 
source /usr/local/qnx/env/bin/activate
source $HOME/qnx800/qnxsdp-env.sh
 
cd $HOME/qnxprojects/my-project/
 
ntoaarch64-gcc -std=c99 -O0 -g \
  -I$QNX_TARGET/usr/include \
  -o acb-tracker \
  acb.c \
  -L$QNX_TARGET/usr/lib -lsqlite3 -lncurses \
  -Wl,-rpath-link,$QNX_TARGET/usr/lib

echo "Built acb-tracker"
