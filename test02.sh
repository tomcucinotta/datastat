#!/bin/bash

# test02: with key

cat test02.txt | head -n 18 | tail -n 11 | shuf > test02_ra.txt
cat test02.txt | head -n 29 | tail -n 6  | shuf > test02_rb.txt
cat test02_ra.txt test02_rb.txt > test02_all.txt
./datastat --no-avg --1qt --2qt --3qt --min --max -k 1-2 test02_all.txt
