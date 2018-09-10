all: datastat

CXXFLAGS=-O3 -Wall -Wextra
#CXXFLAGS=-g -Wall -Wextra -DLOG_DEBUG=1

datastat: datastat.o
	g++ -o $@ $^

tests: datastat
	echo "Running all tests (check test-log.txt for details) ..."
	echo > tests-log.txt; for t in $$(ls test*.sh); do echo -n "Running test $$t... "; if /bin/bash $$t >> tests-log.txt 2>&1; then echo "PASSED"; else echo "FAILED"; fi; done

clean:
	rm -f core *.o *~ test01_*.txt test02_*.txt
