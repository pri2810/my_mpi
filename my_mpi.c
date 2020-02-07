/*
 * ppatel27 Prithvi V Patel
 * my_mpi.c - Custom MPI library
 * CSC548 Spring 2020 HW2 Problem 1.
 * Spec: https://pages.github.ncsu.edu/fmuelle/parsys20/hw/hw2/
 *
 */
#include <pthread.h>
#include <semaphore.h>
#include "my_mpi.h"

sem_t presend; /* for send recv */
sem_t postrecv;
pthread_t server_thread;
int kill_server = 0;
int my_rank;

void *buffer;
ssize_t buffersize_read;
MPI_Datatype recvtype;
char *recvproc;
int Send_Util(const void *sendbuf, int sendcount, MPI_Datatype sendtype, int dest, int tag, MPI_Comm comm);
int Recv_Util(void *recvbuf);

void error(const char *msg)
{
    perror(msg);
	pthread_cancel(server_thread);
    exit(0);
}

/* node_server() is the server thread which is spawned on every node which are reserved by srun.
 * It acts as a server for each node and does the part of listening using a socket. The portname of the server on 
 * each node is 32222+rank of the node.
 * As the server accepts can accept any connection trying to reach it, in order to make sure that the whenever MPI_recv 
 * is called only the processor who is source is to be accepted and hence an ack is sent to source if it is expected now or not.
 * This has been done in a while loop. This, blocking nature of Recv was required as there is no extra information stored
 * in the buffer being received.
 *
 * Two semaphores presend and postrecv are used to read the buffer in a blocking manner and the main thread or the MPI_recv
 * reads the buffer after postrecv is posted (or released by server). presend makes sure that it waits till it doesnot have 
 * read buffers.
*/
void *node_server(void *arg) {

	int server_sockfd, server_portno;
	struct sockaddr_in serv_addr;

    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sockfd < 0) {
    	perror("ERROR opening socket");
    	pthread_exit(NULL);
		exit(0);
    }

	int enable = 1;
	if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&enable, sizeof(enable)) < 0)
    	perror("setsockopt(SO_REUSEADDR) failed");

    bzero((char *) &serv_addr, sizeof(serv_addr));

	server_portno = 32222+my_rank;
   
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(server_portno);

    if (bind(server_sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    	perror("ERROR on binding");
		close(server_sockfd);
		pthread_exit(NULL);
	}
    	
    if((listen(server_sockfd, BACKLOG)) == -1) {
		perror("listen failure\n");
		close(server_sockfd);
		pthread_exit(NULL);
	}

    while (1)
 	{
 		// lock the presend till MPI_Recv or MPI_Sendrecv asks to read. 
		sem_wait(&presend); 
		
		//This is to check whether the server needs to be killed.
        if(kill_server == 1){
			break;
		}
 		int client_newsockfd, n;
 		socklen_t clilen;
 		struct sockaddr_in cli_addr;
		clilen = sizeof(cli_addr);
		char okay = 'n';
		
		//okay='o' is sent when the correct node connects to the server and 
		// the following loop is broken

		while(okay == 'n') {
			client_newsockfd = accept(server_sockfd,(struct sockaddr *) &cli_addr, &clilen);
			if (client_newsockfd < 0) {
				error("ERROR on accept");
				sem_post(&postrecv); 
				close(server_sockfd);
				pthread_exit(NULL);
			}

			struct hostent *he;
			he = gethostbyaddr(&(cli_addr.sin_addr), sizeof(cli_addr.sin_addr), AF_INET);
			char *client_host = he->h_name;
			for(int k = 0; k<strlen(client_host);k++){
				if(client_host[k] == '.'){
					client_host[k] = '\0';
					break;
				}
			}
			if(strcmp(recvproc, client_host)!=0){
				write(client_newsockfd, &okay, 1);
				close(client_newsockfd);
			}
			else {
				okay = 'o';
				write(client_newsockfd, &okay, 1);
				break;
			}
		}
		
		//Once connected to correct node, read the number of bytes as required by recv or sendrecv
		switch (recvtype) {
			case MPI_CHAR: 
				buffer = (char *)malloc(buffersize_read);
				break;
			case MPI_INT:
				buffer = (int *)malloc(buffersize_read);
				break;
			case MPI_DOUBLE:
				buffer = (double *)malloc(buffersize_read);
				break;
			default:
				error("Type not mentioned");
				break;
    	}

		//Read till all bytes are read
		n=0;
		while(n!=buffersize_read){
			int x = read(client_newsockfd, buffer+n, buffersize_read);
			if (x < 0) {
				error("ERROR reading from socket");
				sem_post(&postrecv);
				close(server_sockfd);
				pthread_exit(NULL);
			}
			n += x;
		}
		
		//close the connection and post the postrecv semaphore so that main copies the buffer to 
		//correct location
		close(client_newsockfd);
		sem_post(&postrecv);

 	} 
	close(server_sockfd);
    pthread_exit(NULL);
}

/* Pass the same arguments as received by main from my_prun
 * Arguments passed are nodefile.txt, total number of processors, rank of this node
 * Init reads the nodefile.txt and populates the MPI_COMM_WORLD data structure with the 
 * information of all the nodes and their hostnames.
 * It also spwans the server thread and initiliazes the semaphores and calls the MPI_Barrier before returning.
 */
int MPI_Init( int *argc, char **argv_param[] ) {

	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	
	char **argv = *argv_param;
	FILE *fd = fopen(argv[1], "r");

 	int i = 0;
 	
	if (fd == NULL)
		return MPI_FAIL;
		
	MPI_COMM_WORLD.proc_size = atoi(argv[2]);
	MPI_COMM_WORLD.my_rank = atoi(argv[3]);
	my_rank = MPI_COMM_WORLD.my_rank;

	MPI_COMM_WORLD.allhostname_ptr = (char *)malloc(MPI_COMM_WORLD.proc_size * MAXLENNAME * sizeof(char));
    MPI_COMM_WORLD.allprocnames = (char **)malloc(MPI_COMM_WORLD.proc_size * sizeof(char*));
    if(MPI_COMM_WORLD.allprocnames == NULL){
        perror("allocation error\n");
        exit(0);
    }

    for(i = 0; i < MPI_COMM_WORLD.proc_size; i++){
        MPI_COMM_WORLD.allprocnames[i] = &MPI_COMM_WORLD.allhostname_ptr[i * MAXLENNAME];
    }
	i=0;
    while(fscanf(fd, "%s", MPI_COMM_WORLD.allprocnames[i])){
    	i++;
    }
    MPI_COMM_WORLD.myprocname = MPI_COMM_WORLD.allprocnames[MPI_COMM_WORLD.my_rank];
	
	//Initialize the semaphore for the main thread and the server thread
	sem_init(&presend, 0, 0);
	sem_init(&postrecv, 0, 0);

	//Spawn the server thread
	pthread_create(&server_thread, NULL, node_server, &my_rank);
	
	//Start the program at the same time, so call the barrier
	MPI_Barrier(MPI_COMM_WORLD);

	free(line);
	fclose(fd);

	return MPI_SUCCESS;
}

/* Stores the total count of processors in size */
int MPI_Comm_size( MPI_Comm comm, int *size ) {
	*size = comm.proc_size;
	return MPI_SUCCESS;
}

/* Stores the rank of this processor in rank */
int MPI_Comm_rank( MPI_Comm comm, int *rank ) {
	*rank = comm.my_rank;
	return MPI_SUCCESS;
}

/* A helper util that is used in MPI_Send and MPI_Sendrecv for the purpose of sending the data to destination rank.
 * It tries on connecting to the server of the dest till the dest server sends okay message as 'o'.
 * Once it is done, the sendbuf of sendcount*sendtype bytes is send
 */
int Send_Util(const void *sendbuf, int sendcount, MPI_Datatype sendtype, int dest, int tag, MPI_Comm comm) {

	int my_sockfd, n, portno;
    struct sockaddr_in send_serv_addr;
    struct hostent *send_server;

	int flag = 0;
	char ok_message = 'n';
	
	//Exit the loop if the server accepts the connection.
	//Create a socket which connects to the destination(dest) using the hostname
	// of dest, which is stored in MPI_COMM_WORLD(comm) and the server port no is 32222+dest
	
    while(flag == 0 && ok_message == 'n') {
		my_sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (my_sockfd < 0) {
			perror("ERROR opening socket");
			return MPI_FAIL;
		}
		
		send_server = gethostbyname(comm.allprocnames[dest]);
		portno = 32222+dest;

		if (send_server  == NULL) {
			perror("ERROR, no such host\n");
			return MPI_FAIL;
		}
		
		bzero((char *) &send_serv_addr, sizeof(send_serv_addr));
		send_serv_addr.sin_family = AF_INET;
		bcopy((char *)send_server->h_addr, 
			(char *)&send_serv_addr.sin_addr.s_addr,
			send_server->h_length);
		send_serv_addr.sin_port = htons(portno);

		if(connect(my_sockfd,(struct sockaddr *) &send_serv_addr,sizeof(send_serv_addr)) >= 0)
		{
			read(my_sockfd, &ok_message, 1);	
			if(ok_message == 'o'){
				flag =1;
				break;
			}
			else { 
				close(my_sockfd);
			}	
		}
	}

	//Once the connection is successful, write the data to socket
	//sendtype is defined as number of bytes required for that MPI datatype
	switch (sendtype) {
        case MPI_CHAR: 
			n = write(my_sockfd, (char *)sendbuf, sendtype*sendcount);
            break;
        case MPI_INT:
			n = write(my_sockfd, (int *)sendbuf, sendtype*sendcount);
            break;
        case MPI_DOUBLE:
			n = write(my_sockfd, (double *)sendbuf, sendtype*sendcount);
			break;
        default:
			return MPI_FAIL;
            break;
    }
    if (n < 0) {
		perror("ERROR writing to socket");
		return MPI_FAIL;
	}
	close(my_sockfd);   
	return MPI_SUCCESS;
}


/* The actual send function which uses the Send_Util */
int MPI_Send(const void *sendbuf, int sendcount, MPI_Datatype sendtype, int dest, int tag, MPI_Comm comm) {
	return Send_Util(sendbuf, sendcount, sendtype,dest, tag, comm);
}

/* MPI_Recv uses the semaphores to direct the server to start accepting the connection from the source
 * and start writing it to buffer. 
 * Once server is done, Recv_Util is used to read the buffer.
 */
int MPI_Recv(void *recvbuf, int recvcount, MPI_Datatype recvdatatype, int source, int tag, MPI_Comm comm, MPI_Status *status) {

	buffersize_read = recvcount * recvdatatype;
	recvtype = recvdatatype;
	recvproc = comm.allprocnames[source];	
	sem_post(&presend);
	sem_wait(&postrecv);
	return Recv_Util(recvbuf);
}

/* reads the buffer once server is done reading it from the socket connection with the source processor */
int Recv_Util(void *recvbuf) {

	switch (recvtype) {
		case MPI_CHAR: 
			memcpy((char*)recvbuf, (void*)buffer, buffersize_read);
			break;
		case MPI_INT:
			memcpy((int*)recvbuf, (void*)buffer, buffersize_read);
			break;
		case MPI_DOUBLE:
			memcpy((double*)recvbuf, (void*)buffer, buffersize_read);
			break;
		default:
			return MPI_FAIL;
    }
    //buffer is allocated eveytime dynamically and hence we free it up so it can be used again
 	free(buffer);
	buffersize_read = 0;

    return MPI_SUCCESS;
}

/* Inorder to make it blocking as well as simultaneous, both the source and dest can start sending the message 
 * and the server of both the nodes can start listening at the same time
 * hence the presend semaphore is released before sending. Later Recv_Util is called once server has released
 * semaphore postrecv
 */
int MPI_Sendrecv( const void *sendbuf, int sendcount, MPI_Datatype sendtype, int dest, int sendtag,
	void *recvbuf, int recvcount, MPI_Datatype recvdatatype, int source, int recvtag,
    MPI_Comm comm, MPI_Status *status ){

	buffersize_read = recvcount * recvdatatype;
	recvtype = recvdatatype;
	recvproc = comm.allprocnames[source];	

	sem_post(&presend);

	int success = Send_Util(sendbuf, sendcount, sendtype,dest, sendtag, comm);

	sem_wait(&postrecv);
	return Recv_Util(recvbuf);
}

/* The Master/ 0th rank node sends a message to all other nodes */
int MPI_Barrier( MPI_Comm comm ) {
	int i = 1;
	MPI_Status status;

	if(my_rank == 0)
	{
		for(int j = 1; j<comm.proc_size;j++)
		MPI_Send(&i,1,MPI_INT,j,123,MPI_COMM_WORLD);
	}
	else
	{
		MPI_Recv(&i,1,MPI_INT,0,123,MPI_COMM_WORLD,&status);
	}

	return MPI_SUCCESS;
}

/* Call MPI_Barrier so that all processes are in sync and then kill the server thread
 * on each node and complete neatly exit the process
 */
int MPI_Finalize( void ){

    MPI_Barrier( MPI_COMM_WORLD );
	kill_server = 1;
    sem_post(&presend);
	pthread_join(server_thread,NULL);
	sem_destroy(&presend);
	sem_destroy(&postrecv); 
	return MPI_SUCCESS;
}
