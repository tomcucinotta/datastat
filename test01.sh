#!/bin/bash

# test01: no key

cat test01.txt | head -n 18 | tail -n 11 | shuf > test01_all.txt
out=$(./datastat --no-avg --1qt --2qt --3qt --min --max test01_all.txt -nh)

if [ "$out" == "25.5 40 42.5 6 49" ]; then
    true;
else
    false;
fi
