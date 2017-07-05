#! /bin/bash

pressurelevel="$1"
duration="$2"
echo "Stressing all CPUs at $pressurelevel% capacity for $duration"s...
stress-ng -c 0 -l $pressurelevel -t $duration
