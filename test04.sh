#!/bin/bash

# avg with key

out1=$(echo -e "a 1 2 3\nb 4 5 6\na 7 8 9\nb 10 11 12" | ./datastat -k 1 | tail -n +2 | head -1)
out2=$(echo -e "a 1 2 3\nb 4 5 6\na 7 8 9\nb 10 11 12" | ./datastat -k 1 | tail -n +3 | head -1)
echo out1=$out1
echo out2=$out2
if [ "$out1" == "a 4 5 6" -a "$out2" == "b 7 8 9" ]; then
    true;
else
    false;
fi
