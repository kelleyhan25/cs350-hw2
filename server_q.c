/*******************************************************************************
* Simple FIFO Order Server Implementation
*
* Description:
*     A server implementation designed to process client requests in First In,
*     First Out (FIFO) order. The server binds to the specified port number
*     provided as a parameter upon launch.
*
* Usage:
*     <build directory>/server <port_number>
*
* Parameters:
*     port_number - The port number to bind the server to.
*
* Author:
*     Renato Mancuso
*
* Affiliation:
*     Boston University
*
* Creation Date:
*     September 10, 2024
*
* Last Changes:
*     September 22, 2024
*
* Notes:
*     Ensure to have proper permissions and available port before running the
*     server. The server relies on a FIFO mechanism to handle requests, thus
*     guaranteeing the order of processing. For debugging or more details, refer
*     to the accompanying documentation and logs.
*
* Kelley Han Notes: 
* I originally implemented and tested with my queue as a linked list, however, I could not figure it out for the life of me why
* it was stuck in an infinite loop, for only certain values. Redid my queue implementation using a array queue, following these resources found online: 
* https://www.geeksforgeeks.org/queue-in-c/
* https://www.w3resource.com/c-programming-exercises/structure/c-structure-exercises-7.php
*
*
*******************************************************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sched.h>
#include <signal.h>
#include <pthread.h>

/* Needed for wait(...) */
#include <sys/types.h>
#include <sys/wait.h>

/* Needed for semaphores */
#include <semaphore.h>

/* Include struct definitions and other libraries that need to be
 * included by both client and server */
#include "common.h"

#define BACKLOG_COUNT 100
#define USAGE_STRING				\
	"Missing parameter. Exiting.\n"		\
	"Usage: %s <port_number>\n"

/* 4KB of stack for the worker thread */
#define STACK_SIZE (4096)

/* START - Variables needed to protect the shared queue. DO NOT TOUCH */
sem_t * queue_mutex;
sem_t * queue_notify;
/* END - Variables needed to protect the shared queue. DO NOT TOUCH */

/* Max number of requests that can be queued */
#define QUEUE_SIZE 500

struct queue {
    /* IMPLEMENT ME */
    struct request_meta req_items[QUEUE_SIZE];
    int head; 
    int tail; 
    int queue_size; 
};

struct worker_params {
    /* IMPLEMENT ME */
    struct queue * the_queue;
    int conn_socket; 
};

volatile int worker_terminated; 

/* Add a new request <request> to the shared queue <the_queue> */
int add_to_queue(struct request_meta to_add, struct queue * the_queue)
{
	
	int retval = 0;
	/* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
	sem_wait(queue_mutex);
	/* QUEUE PROTECTION INTRO END --- DO NOT TOUCH */

	/* WRITE YOUR CODE HERE! */
	/* MAKE SURE NOT TO RETURN WITHOUT GOING THROUGH THE OUTRO CODE! */ 
	if (the_queue->queue_size <= QUEUE_SIZE - 1) {
		the_queue->req_items[the_queue->tail] = to_add; 
		the_queue->tail = (the_queue->tail + 1) % QUEUE_SIZE; 
		the_queue->queue_size++; 
	} else {
		retval = -1; 
	}

	/* QUEUE PROTECTION OUTRO START --- DO NOT TOUCH */
	sem_post(queue_mutex);
	sem_post(queue_notify);
	/* QUEUE PROTECTION OUTRO END --- DO NOT TOUCH */
	return retval;
}

/* Add a new request <request> to the shared queue <the_queue> */
struct request_meta get_from_queue(struct queue * the_queue)
{
	struct request_meta retval;
	/* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
	sem_wait(queue_notify);
	sem_wait(queue_mutex);
	/* QUEUE PROTECTION INTRO END --- DO NOT TOUCH */
	
	/* WRITE YOUR CODE HERE! */
	/* MAKE SURE NOT TO RETURN WITHOUT GOING THROUGH THE OUTRO CODE! */
	
	
	retval = the_queue->req_items[the_queue->head];
	the_queue->head = (the_queue->head + 1) % QUEUE_SIZE; 
	the_queue->queue_size--; 
		

	/* QUEUE PROTECTION OUTRO START --- DO NOT TOUCH */
	sem_post(queue_mutex);
	/* QUEUE PROTECTION OUTRO END --- DO NOT TOUCH */
	return retval;
}

/* Implement this method to correctly dump the status of the queue
 * following the format Q:[R<request ID>,R<request ID>,...] */
void dump_queue_status(struct queue * the_queue)
{
	int i = 0;
	/* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
	sem_wait(queue_mutex);
	/* QUEUE PROTECTION INTRO END --- DO NOT TOUCH */

	/* WRITE YOUR CODE HERE! */
	/* MAKE SURE NOT TO RETURN WITHOUT GOING THROUGH THE OUTRO CODE! */
	printf("Q:[");
	if (the_queue->queue_size > 0) {
		while(i < the_queue->queue_size) {
			printf("R%lu", the_queue->req_items[(the_queue->head + i) % QUEUE_SIZE].request.request_id);

			if (i < the_queue->queue_size - 1) {
				printf(",");
			}
			i++; 
		}
	}
	printf("]\n");


	/* QUEUE PROTECTION OUTRO START --- DO NOT TOUCH */
	sem_post(queue_mutex);
	/* QUEUE PROTECTION OUTRO END --- DO NOT TOUCH */
}


/* Main logic of the worker thread */
/* IMPLEMENT HERE THE MAIN FUNCTION OF THE WORKER */
void* worker_main(void* arg) {
    struct worker_params  *work_parm = (struct worker_params*)arg; 
    int conn_socket = work_parm->conn_socket; 
	struct response res; 
	struct timespec start_timestamp; 
	struct timespec completion_timestamp; 
	struct request_meta new_request; 
	struct timespec sent_timestamp;

    while(worker_terminated == 0) {
		
        new_request = get_from_queue(work_parm->the_queue);
		
        if (worker_terminated == 1) {
			
            break; 
        }


		sent_timestamp = new_request.request.timestamp; 
	
		clock_gettime(CLOCK_MONOTONIC, &start_timestamp);
		
		get_elapsed_busywait(new_request.request.length.tv_sec, new_request.request.length.tv_nsec);
		//busywait_timespec(new_request.request.length);
        
		clock_gettime(CLOCK_MONOTONIC, &completion_timestamp);

		res.req_id = new_request.request.request_id; 
		res.reserved_field = 0; 
		res.acknowledgement_value = 0; 

		send(conn_socket, &res, sizeof(res), 0);


		double sent = sent_timestamp.tv_sec + (double)sent_timestamp.tv_nsec / NANO_IN_SEC; 
		double request_length = new_request.request.length.tv_sec + (double)new_request.request.length.tv_nsec / NANO_IN_SEC; 
		double receipt_timestamp_dc = new_request.receipt_timestamp.tv_sec + (double)new_request.receipt_timestamp.tv_nsec / NANO_IN_SEC; 
		double start_timestamp_dc = start_timestamp.tv_sec + (double)start_timestamp.tv_nsec / NANO_IN_SEC; 
		double completion_timestamp_dc = completion_timestamp.tv_sec + (double)completion_timestamp.tv_nsec / NANO_IN_SEC; 

		printf("R%lu:%.9lf,%.9lf,%.9lf,%.9lf,%.9lf\n", new_request.request.request_id, sent, request_length, receipt_timestamp_dc, start_timestamp_dc, completion_timestamp_dc);
		dump_queue_status(work_parm->the_queue);
    }

    return NULL; 
}

/* Main function to handle connection with the client. This function
 * takes in input conn_socket and returns only when the connection
 * with the client is interrupted. */
void handle_connection(int conn_socket)
{
	struct request_meta *request_meta;
	struct queue * the_queue;
	size_t in_bytes;
	struct worker_params *work_parm; 
	

	/* The connection with the client is alive here. Let's
	 * initialize the shared queue. */

	/* IMPLEMENT HERE ANY QUEUE INITIALIZATION LOGIC */
	request_meta = (struct request_meta *)malloc(sizeof(struct request_meta));
	if (request_meta == NULL) {
		perror("Failed");
		goto cleanup; 
	}
	the_queue = (struct queue*)malloc(sizeof(struct queue));
	if (the_queue == NULL) {
		perror("Failed");
		goto cleanup; 
	}
	the_queue->head = 0; 
	the_queue->tail = 0; 
	the_queue->queue_size = 0; 

	work_parm = (struct worker_params*)malloc(sizeof(struct worker_params));
	if (work_parm == NULL) {
		perror("failed");
		goto cleanup;  
	}
	work_parm->conn_socket = conn_socket; 
	work_parm->the_queue = the_queue; 

	/* Queue ready to go here. Let's start the worker thread. */
	pthread_t worker; 

	/* IMPLEMENT HERE THE LOGIC TO START THE WORKER THREAD. */
	int connection_exists = pthread_create(&worker, NULL, worker_main, work_parm);
	if (connection_exists != 0) {
		perror("Failed");
		goto cleanup; 
	}
	

	/* We are ready to proceed with the rest of the request
	 * handling logic. */

	/* REUSE LOGIC FROM HW1 TO HANDLE THE PACKETS */

	do {
		in_bytes = recv(conn_socket, &request_meta->request, sizeof(struct request), 0);
		if (worker_terminated == 1) {
            break; 
        }
		if(in_bytes <= 0) {
			worker_terminated = 1; 
			break; 
		}
		
		clock_gettime(CLOCK_MONOTONIC, &(request_meta->receipt_timestamp));
		

		/* Don't just return if in_bytes is 0 or -1. Instead
		 * skip the response and break out of the loop in an
		 * orderly fashion so that we can de-allocate the req
		 * and resp varaibles, and shutdown the socket. */

		
		
		if (in_bytes > 0) {
			add_to_queue(*request_meta, the_queue);
		}
		
	} while (in_bytes > 0);

	/* PERFORM ORDERLY DEALLOCATION AND OUTRO HERE */
	
	/* Ask the worker thead to terminate */
	/* ASSERT TERMINATION FLAG FOR THE WORKER THREAD */
	worker_terminated = 1;
	
	/* Make sure to wake-up any thread left stuck waiting for items in the queue. DO NOT TOUCH */
	sem_post(queue_notify);
	/* Wait for orderly termination of the worker thread */	
	/* ADD HERE LOGIC TO WAIT FOR TERMINATION OF WORKER */
	pthread_join(worker, NULL);
	goto cleanup; 
	/* FREE UP DATA STRUCTURES AND SHUTDOWN CONNECTION WITH CLIENT */
	cleanup:
		if (request_meta != NULL) {
			free(request_meta);
		}
		if (the_queue != NULL) {
			free(the_queue);
		}
		if (work_parm != NULL) {
			free(work_parm);
		}

	close(conn_socket);
}


/* Template implementation of the main function for the FIFO
 * server. The server must accept in input a command line parameter
 * with the <port number> to bind the server to. */
int main (int argc, char ** argv) {
	int sockfd, retval, accepted, optval;
	in_port_t socket_port;
	struct sockaddr_in addr, client;
	struct in_addr any_address;
	socklen_t client_len;

	/* Get port to bind our socket to */
	if (argc > 1) {
		socket_port = strtol(argv[1], NULL, 10);
		printf("INFO: setting server port as: %d\n", socket_port);
	} else {
		ERROR_INFO();
		fprintf(stderr, USAGE_STRING, argv[0]);
		return EXIT_FAILURE;
	}

	/* Now onward to create the right type of socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0) {
		ERROR_INFO();
		perror("Unable to create socket");
		return EXIT_FAILURE;
	}

	/* Before moving forward, set socket to reuse address */
	optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&optval, sizeof(optval));

	/* Convert INADDR_ANY into network byte order */
	any_address.s_addr = htonl(INADDR_ANY);

	/* Time to bind the socket to the right port  */
	addr.sin_family = AF_INET;
	addr.sin_port = htons(socket_port);
	addr.sin_addr = any_address;

	/* Attempt to bind the socket with the given parameters */
	retval = bind(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));

	if (retval < 0) {
		ERROR_INFO();
		perror("Unable to bind socket");
		return EXIT_FAILURE;
	}

	/* Let us now proceed to set the server to listen on the selected port */
	retval = listen(sockfd, BACKLOG_COUNT);

	if (retval < 0) {
		ERROR_INFO();
		perror("Unable to listen on socket");
		return EXIT_FAILURE;
	}

	/* Ready to accept connections! */
	printf("INFO: Waiting for incoming connection...\n");
	client_len = sizeof(struct sockaddr_in);
	accepted = accept(sockfd, (struct sockaddr *)&client, &client_len);

	if (accepted == -1) {
		ERROR_INFO();
		perror("Unable to accept connections");
		return EXIT_FAILURE;
	}

	/* Initialize queue protection variables. DO NOT TOUCH. */
	queue_mutex = (sem_t *)malloc(sizeof(sem_t));
	queue_notify = (sem_t *)malloc(sizeof(sem_t));
	retval = sem_init(queue_mutex, 0, 1);
	if (retval < 0) {
		ERROR_INFO();
		perror("Unable to initialize queue mutex");
		return EXIT_FAILURE;
	}
	retval = sem_init(queue_notify, 0, 0);
	if (retval < 0) {
		ERROR_INFO();
		perror("Unable to initialize queue notify");
		return EXIT_FAILURE;
	}
	/* DONE - Initialize queue protection variables. DO NOT TOUCH */

	/* Ready to handle the new connection with the client. */
	handle_connection(accepted);

	free(queue_mutex);
	free(queue_notify);

	close(sockfd);
	return EXIT_SUCCESS;

}
