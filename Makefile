all: datastat

CXXFLAGS=-O3 -Wall -Wextra
#CXXFLAGS=-g -Wall -Wextra

datastat: datastat.o
	g++ -o $@ $^ -lreadline

tests: datastat test01 test02

# test01: no key
test01: 
	cat test01.txt | head -n 18 | tail -n 11 | shuf > test01_all.txt
	./datastat --no-avg --1qt --2qt --3qt --min --max test01_all.txt

# test02: with key
test02: 
	cat test02.txt | head -n 18 | tail -n 11 | shuf > test02_ra.txt
	cat test02.txt | head -n 29 | tail -n 6  | shuf > test02_rb.txt
	cat test02_ra.txt test02_rb.txt > test02_all.txt
	./datastat --no-avg --1qt --2qt --3qt --min --max -k 1-2 test02_all.txt

clean:
	rm -f core *.o *~ test01_*.txt test02_*.txt
