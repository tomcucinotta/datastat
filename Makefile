all: datastat

CXXFLAGS=-O3 -Wall -Wextra
#CXXFLAGS=-g -Wall -Wextra -DLOG_DEBUG=1

datastat: datastat.o
	g++ -o $@ $^

tests: datastat
	echo "Running all tests (check test-log.txt for details) ..."
	echo > tests-log.txt; for t in $$(ls test*.sh); do echo -n "Running test $$t... "; if /bin/bash $$t >> tests-log.txt 2>&1; then echo "\033[0;32mPASSED\033[0;0m"; else echo "\033[0;31mFAILED\033[0;0m"; fi; done

clean:
	rm -f core *.o *~ test01_*.txt test02_*.txt
