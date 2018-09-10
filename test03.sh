#!/bin/bash

out=$(echo -e "1 2 3\n4 5 6\n7 8 9" | ./datastat | tail -n +2)
echo out=$out
if [ "$out" == "4 5 6" ]; then
    true;
else
    false;
fi
