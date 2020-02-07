/*
 * my_rtt.c
 * ppatel27 Prithvi V Patel
 * my_rtt.c - Custom MPI library
 * CSC548 Spring 2020 HW2 Problem 1.
 * Spec: https://pages.github.ncsu.edu/fmuelle/parsys20/hw/hw2/
 *
 */
#include <time.h>
#include <stdlib.h>
#include "my_mpi.h"
#include <math.h>
#include <stdio.h>
#include <sys/time.h>

int main(int argc, char *argv[])
{
	// iter defines the number of iterataions required to average the RTT
	//p: Number of processors in the communicator of MPI
	//rank: Processor ID (here Node ID as each copy of the program is run on a node)
    int rank, p, iter = 10;
    long sum_time, time[iter];
	MPI_Status status;
	
	//The program uses gettimeofday() to calculate the time elapsed.
	struct timeval start, end;
		
	MPI_Init(&argc,&argv);
	MPI_Comm_size(MPI_COMM_WORLD, &p);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	
	//Using character array to send and receive data as each character is 1 Byte
	//The size of the character array varies from 2^5(32B) to 2^21(2MB)
	for (int power = 5; power < 22; power++){
		int size = pow(2, power);
		
		//buffer is the buffer being sent.
		//buffer2 is the buffer being received.
		char *buffer , *buffer2;
		buffer = (char *)malloc(size);
		buffer2 = (char *)malloc(size);
		
		//Send receive occurs between a pair of nodes in such a way that
		// an even numbered node communicates with the immediate next odd numbered node
		//Thus, each pair can be said to be 2i, 2i+1 ; where i=0,1,2,3 (when ran on 8 nodes)
		
		//Even numbered node sends buffer and receives buffer2 of equal size from immediate next
		//odd numbered node.
		
		//Time is calculated between just before and after the sendrecv call
		//As Sendrecv is blocking call, we nearly get the RTT of sending and receiving equal number of bytes
		//between a pair.
		if(rank % 2 == 0) {
		
		    for(int i = 0; i < iter+2; i++)
		    {
		    	gettimeofday(&start,NULL);
		        MPI_Sendrecv(&buffer[0], size, MPI_CHAR, rank+1, 123, &buffer2[0], size, MPI_CHAR, rank+1, 123, MPI_COMM_WORLD, &status);
		        gettimeofday(&end,NULL);
		            
		        //Skipping the first two iterations as they were anomalies/outliers in the record
		        //These could be due to the network messages or cache not being cleared up.
		        if(i>1)
		        {
		        	long tmp = ((end.tv_sec-start.tv_sec)*1000000 + (end.tv_usec-start.tv_usec));
		          	sum_time += tmp;
		           	time[i-2] = tmp;
		    	}
		    }
		}
		//The same process as above is adopted for odd numbered node which sends message to even numbered(1 less than itself)
		//and also receives a message from it in buffer2
		else {
		    for(int i = 0; i < iter+2; i++)
		    {
		    	gettimeofday(&start,NULL);
		        MPI_Sendrecv(&buffer[0], size, MPI_CHAR, rank-1, 123, &buffer2[0], size, MPI_CHAR, rank-1, 123, MPI_COMM_WORLD, &status);
		        gettimeofday(&end,NULL);
		        
		        if(i>1)
		        {
					long tmp = ((end.tv_sec-start.tv_sec)*1000000 + (end.tv_usec-start.tv_usec));
					sum_time += tmp;
		            time[i-2] = tmp;
		        }
		    }
		}

		//Evaluating the average and std of RTT.
		//std denotes standard deviation
		double avg = (double)sum_time/(double)iter;
		double std = 0.0;
		for(int i = 0; i < iter; i++) {
		    std += pow((double)time[i]-avg, 2.0);
		}
		
		std = sqrt(std/(double)iter);
		avg = avg/1000000.0;
		std /= 1000000.0;
		//printf("%d %lf %lf %d\n",size, avg, std, rank);
		//fflush(stdout);

		//Sending the average and std RTT to the controller that is rank 0
		//Rank 0 receives all the average RTT and std RTT.
		//The output printed is of the format:
		//[message size1] [node_0_average] [node_0_stddev] [node_1_average] [node_1_stddev] ....
		//[message size2] [node_0_average] [node_0_stddev] [node_1_average] [node_1_stddev] .... 
		//...
		//...
        
		if(rank != 0)
		{
		    MPI_Send(&avg, 1, MPI_DOUBLE, 0, 123, MPI_COMM_WORLD);
		    MPI_Send(&std, 1, MPI_DOUBLE, 0, 123, MPI_COMM_WORLD);
		}
		else
		{
			
        	printf("%d ",size);
			fflush(stdout);        	
		    double avgr[p],stdr[p];
		    avgr[0] = avg;
		    stdr[0] = std;
		    for(int i = 1; i < p; i++)
		    {
		            MPI_Recv(&avgr[i], 1, MPI_DOUBLE, i, 123, MPI_COMM_WORLD, &status);
		            MPI_Recv(&stdr[i], 1, MPI_DOUBLE, i, 123, MPI_COMM_WORLD, &status);
		    }
		    for(int i = 0; i < p; i++)
		    {
		            printf("%lf %lf ", avgr[i], stdr[i]);
					fflush(stdout); 
		    }
		    printf("\n");
			fflush(stdout); 
		}
		
	}
    MPI_Finalize();
    return 0;
}

