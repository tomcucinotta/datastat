all: datastat

CXXFLAGS=-O3 -Wall

datastat: datastat.o
	g++ -o $@ $^ -lreadline

clean:
	rm -f *.o *~ datastat
