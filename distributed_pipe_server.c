/*
 * This is sample code generated by rpcgen.
 * These are only templates and you can use them
 * as a guideline for developing your own functions.
 */

#include "distributed_pipe.h"

#define CYCLICSIZE 10
#define BUFSIZE 512

static char* bufor[CYCLICSIZE] = { NULL };
static int writeIndex = 0;
static int readIndex = 0;

int *
write_17_svc(char **argp, struct svc_req *rqstp)
{
	static int result = 0;
	
	if (bufor[writeIndex] == NULL) {
		bufor[writeIndex] = malloc(BUFSIZE);
		strncpy(bufor[writeIndex], *argp, BUFSIZE);
		if (writeIndex < CYCLICSIZE-1) {
			writeIndex++;
		} else {
			writeIndex = 0;
		}
		result = 0;
		printf("zapisalem\n");
	} else {
		result = -1;
		printf("nie zapisalem\n");
	}
	
	return &result;
}

char **
read_17_svc(void *argp, struct svc_req *rqstp)
{
	static char * result;
	
	if (bufor[readIndex] != NULL) {
		result = malloc(BUFSIZE);
		strncpy(result, bufor[readIndex], BUFSIZE);
		free(bufor[readIndex]);
		bufor[readIndex] = NULL;
		if (readIndex < CYCLICSIZE-1) {
			readIndex++;
		} else {
			readIndex = 0;
		}
		printf("odczytalem\n");
	} else {
		result = NULL;
		printf("nie odczytalem\n");
	}

	return &result;
}

int *
ping_17_svc(void *argp, struct svc_req *rqstp)
{
	static int result = 0;
	return &result;
}