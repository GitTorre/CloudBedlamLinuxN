#! /bin/bash

# nice and simple... :) -CT
# bash stress-mem.sh 90 15 # eat 90% of available memory, stop after 15 seconds...

vmbytes=$(awk "/MemFree/ { printf \"%d\", (\$2*($1/100))/1024/1024 }" /proc/meminfo)G

# This requires stress, try to install, suppress output...
apt install stress  2> /dev/null > /dev/null

echo "Eating $vmbytes of available memory for $2"s...
stress --vm 1 --vm-bytes ${vmbytes} --vm-hang $2 --vm-keep --timeout $2
