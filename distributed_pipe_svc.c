#include "distributed_pipe.h"
#include <stdio.h>
#include <stdlib.h>
#include <rpc/pmap_clnt.h>
#include <string.h>
#include <memory.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifndef SIG_PF
#define SIG_PF void(*)(int)
#endif

#define BUFSIZE 512

CLIENT ** servers;
int servers_num;

char buf[BUFSIZE];

static int convert_xdr_to_int(char * xdrstr) {
	XDR xdr;
	int decoded_result;
	xdrmem_create(&xdr, xdrstr, sizeof(int), XDR_DECODE);
	if (!xdr_int(&xdr, &decoded_result)) {
		printf ("Error decoding result!\n");
		decoded_result = -1;
	}
	xdr_destroy(&xdr);
	return decoded_result;
}

static char * convert_xdr_to_string(char * xdrstr) {
	XDR xdr;
	char * decoded_result = buf;
	xdrmem_create(&xdr, xdrstr, BUFSIZE, XDR_DECODE);
	if (!xdr_pointer(&xdr, &decoded_result, BUFSIZE, (xdrproc_t) xdr_wrapstring)) {
		printf ("Error decoding result!\n");
		decoded_result = NULL;
	}
	xdr_destroy(&xdr);
	return decoded_result;
}

static void
do_replication(int method, char * arg, char * result) {
	if (method == write) {
		int decoded_result = convert_xdr_to_int(result);
		if (decoded_result == 0) {
			for (int i = 0; i < servers_num; i++) {
				replicate_write_17(&arg, servers[i]);
			}
		}
	} else if (method == read) {
		char * decoded_result = convert_xdr_to_string(result);
		if (decoded_result != NULL) {
			for (int i = 0; i < servers_num; i++) {
				replicate_read_17((void*)&arg, servers[i]);
			}
		}
	}
}

static void
rp_17_svc(struct svc_req *rqstp, register SVCXPRT *transp)
{
	union {
		char *write_17_arg;
	} argument;
	char *result;
	xdrproc_t _xdr_argument, _xdr_result;
	char *(*local)(char *, struct svc_req *);

	switch (rqstp->rq_proc) {
	case NULLPROC:
		(void) svc_sendreply (transp, (xdrproc_t) xdr_void, (char *)NULL);
		return;

	case write:
		_xdr_argument = (xdrproc_t) xdr_wrapstring;
		_xdr_result = (xdrproc_t) xdr_int;
		local = (char *(*)(char *, struct svc_req *)) write_17_svc;
		break;

	case read:
		_xdr_argument = (xdrproc_t) xdr_void;
		_xdr_result = (xdrproc_t) xdr_wrapstring;
		local = (char *(*)(char *, struct svc_req *)) read_17_svc;
		break;
		
	case ping:
		_xdr_argument = (xdrproc_t) xdr_void;
		_xdr_result = (xdrproc_t) xdr_int;
		local = (char *(*)(char *, struct svc_req *)) ping_17_svc;
		break;
		
	case replicate_write:
		_xdr_argument = (xdrproc_t) xdr_wrapstring;
		_xdr_result = (xdrproc_t) xdr_int;
		local = (char *(*)(char *, struct svc_req *)) replicate_write_17_svc;
		break;
		
	case replicate_read:
		_xdr_argument = (xdrproc_t) xdr_void;
		_xdr_result = (xdrproc_t) xdr_int;
		local = (char *(*)(char *, struct svc_req *)) replicate_read_17_svc;
		break;

	default:
		svcerr_noproc (transp);
		return;
	}
	memset ((char *)&argument, 0, sizeof (argument));
	if (!svc_getargs (transp, (xdrproc_t) _xdr_argument, (caddr_t) &argument)) {
		svcerr_decode (transp);
		return;
	}
	result = (*local)((char *)&argument, rqstp);
	
	do_replication(rqstp->rq_proc, argument.write_17_arg, result);
	
	if (result != NULL && !svc_sendreply(transp, (xdrproc_t) _xdr_result, result)) {
		svcerr_systemerr (transp);
	}
	if (!svc_freeargs (transp, (xdrproc_t) _xdr_argument, (caddr_t) &argument)) {
		fprintf (stderr, "%s", "unable to free arguments");
		exit (1);
	}
	return;
}

int
main (int argc, char **argv)
{
	if (argc < 2) {
		printf ("usage: %s config_file\n", argv[0]);
		exit (1);
	}
	
	FILE *fp;
	fp = fopen(argv[1], "r");
	int port;
	fscanf(fp, "%d\n", &port);
	fscanf(fp, "%d\n", &servers_num);
	
	servers = malloc(servers_num * sizeof(CLIENT *));

	int s_port;
	char s_host[255];	
	for (int i = 0; i < servers_num; i++) {
		fscanf(fp, "%s %d\n", s_host, &s_port);
		servers[i] = rp_17 (s_host, s_port);
	}
	
	register SVCXPRT *transp;

	//~ pmap_unset (RP, V1);
	
	int sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == -1) { perror ("socket"); exit (1); }

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons (port);
	addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK); // listen only on 127.0.0.1
	if (bind (sock, (struct sockaddr *) &addr, sizeof addr) == -1)
    { perror ("bind"); exit (1); }

	transp = svcudp_create(sock);
	if (transp == NULL) {
		fprintf (stderr, "%s", "cannot create udp service.\n");
		exit(1);
	}
	if (!svc_register(transp, RP, V1, rp_17_svc, 0)) {
		fprintf (stderr, "%s", "unable to register (RP, V1, udp).\n");
		exit(1);
	}

	//~ transp = svctcp_create(RPC_ANYSOCK, 0, 0);
	//~ if (transp == NULL) {
		//~ fprintf (stderr, "%s", "cannot create tcp service.");
		//~ exit(1);
	//~ }
	//~ if (!svc_register(transp, RP, V1, rp_17, IPPROTO_TCP)) {
		//~ fprintf (stderr, "%s", "unable to register (RP, V1, tcp).");
		//~ exit(1);
	//~ }

	svc_run ();
	fprintf (stderr, "%s", "svc_run returned\n");
	exit (1);
	/* NOTREACHED */
}
