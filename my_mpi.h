/*
 * my_mpi.h
 * ppatel27 Prithvi V Patel
 * my_mpi.h - Custom MPI library
 * CSC548 Spring 2020 HW2 Problem 1.
 * Spec: https://pages.github.ncsu.edu/fmuelle/parsys20/hw/hw2/
 * All the necessary MPI definitions and socket programming headers
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

typedef int MPI_Datatype;
typedef int MPI_Status;

#define MPI_CHAR 1
#define MPI_INT 4
#define MPI_DOUBLE 8

#define MPI_SUCCESS 1
#define MPI_FAIL 0
#define BACKLOG 200
//#define DEBUG 0

#define MAXLENNAME 5

typedef struct mpi_Comm{
        int proc_size;
        int my_rank;
        char *myprocname;
        char **allprocnames;
        char *allhostname_ptr;
}MPI_Comm;


MPI_Comm MPI_COMM_WORLD;


int MPI_Init( int *argc, char **argv[] );

int MPI_Comm_size( MPI_Comm comm, int *size );

int MPI_Comm_rank( MPI_Comm comm, int *rank );

int MPI_Sendrecv( const void *sendbuf, int sendcount, MPI_Datatype sendtype, int dest, int sendtag,
	void *recvbuf, int recvcount, MPI_Datatype recvtype, int source, int recvtag,
    MPI_Comm comm, MPI_Status *status );
    
int MPI_Send(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm);

int MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status);
    
int MPI_Barrier( MPI_Comm comm );

int MPI_Finalize( void );
