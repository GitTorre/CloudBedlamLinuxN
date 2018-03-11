#! /bin/bash

pressurelevel="$1"
duration="$2"
# try and install iproute2, suppress output...
apt install stress-ng  2> /dev/null > /dev/null

echo "Stressing all CPUs at $pressurelevel% capacity for $duration"s...
stress-ng -c 0 -l ${pressurelevel} -t ${duration}
