#  ppatel27  Prithvi V Patel
#  p1.Makefile - Custom MPI Library
#  CSC548 Spring 2020 HW2 Problem 1.
#  Spec: https://pages.github.ncsu.edu/fmuelle/parsys20/hw/hw2/

all: my_rtt
my_rtt: my_mpi.c my_rtt.c
	gcc -lm my_mpi.c my_rtt.c -o my_rtt -lrt -lpthread
clean:
	rm -f my_rtt
