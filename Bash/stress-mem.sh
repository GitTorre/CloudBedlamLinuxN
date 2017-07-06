#! /bin/bash

# nice and simple... :) -CT
# bash stress-mem.sh 90 15 # eat 90% of available memory, stop after 15 seconds...

vmbytes=$(awk "/MemFree/ { printf \"%d\", (\$2*0.$1)/1024/1024; }" /proc/meminfo)G

# This requires stress, try and install, swallow any errors (like if it's already installed, for example...)
apt install stress  2> /dev/null > /dev/null
echo "Eating $1% of available memory for $2"s...
stress --vm 2 --vm-bytes $vmbytes --vm-hang $2 --vm-keep --timeout $2
