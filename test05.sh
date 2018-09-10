#!/bin/bash

# avg with nan

out=$(echo -e "nan 2 3\n4 inf 6\n7 8 nan" | ./datastat -nh --use-nan)
echo out=$out
if [ "$out" == "5.5 5 4.5" ]; then
    true;
else
    false;
fi
