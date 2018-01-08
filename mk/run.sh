#!/bin/sh

make
linux_proc_banner=$(sudo cat /proc/kallsyms | grep linux_proc_banner | sed 's/ .*//')
./testmd $linux_proc_banner 32
