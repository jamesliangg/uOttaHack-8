#!/bin/bash
 
source /usr/local/qnx/env/bin/activate
source $HOME/qnx800/qnxsdp-env.sh
 
cd $HOME/qnxprojects/my-project/
 
ntoaarch64-gcc -o my-project -Igpio/aarch64/ my-project.c
