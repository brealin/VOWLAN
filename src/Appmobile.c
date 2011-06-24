/* Appmobile.c  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "../include/Util.h"
#include "../include/VoIPpkt.h"
#include "../include/CheckPkt.h"
#include "../include/PktGen.h"

/* #define VICDEBUG */

#define DIMPKT 400

fd_set rdset, all;
int fds[2];
int maxfd;
int numduplicati=0;
int numok=0;
int numpersi=0;
int numritardi=0;
int printed=0;
uint32_t idmsg=0;
uint32_t idlastrecv=-1;
FILE *f=NULL;

void sig_close(int signo)
{
	long int id;
	
	for(id=0;id<idlastrecv;id++)
	{
		if(	check_pkt_recv_at_Mobile(id) )
			fprintf(f,"%lu %d\n", id, GetpktrecvMobileDelay(id) );
		else
			fprintf(f,"%lu %d\n", id, -100 );
	}
	if(f!=NULL) 
	{
		fclose(f);
		f=NULL;
	}
	if(signo==SIGINT)				printf("SIGINT\n");
	else if(signo==SIGHUP)	printf("SIGHUP\n");
	else if(signo==SIGTERM)	printf("SIGTERM\n");
	else										printf("other signo\n");
	printf("\n");

	/*fprintf(stderr,"numspediti %d numbytespediti %d\n", idmsg, idmsg*PKTSIZE);*/
	fprintf(stderr,"numspediti %d numbytespediti %d\n", idmsg, idmsg*DIMPKT);
	fprintf(stderr,"numrecvok %d  - numritardi %d  - numpersi %d  - numduplicati %d\n",
			numok, numritardi, idlastrecv-numritardi-numok, numduplicati );
	fflush(stderr);
	exit(888);
}

void EExit(int num)
{
	sig_close(num);
}

long int compute_delay(uint32_t *buf)
{
	struct timeval sent, now, tempoimpiegato;
	uint32_t id;
	long int msec;

	id=buf[0];
	memcpy( (char*)&sent, (char*)&(buf[1]), sizeof(struct timeval) );
	gettimeofday(&now,NULL);
	/*
	fprintf( f,	"\nsent: %lu : %ld sec %ld usec\n", id, sent.tv_sec, sent.tv_usec );
	fprintf( f,	"now:  %lu : %ld sec %ld usec\n", id, now.tv_sec, now.tv_usec );
	*/
	tempoimpiegato=differenza(now,sent);
	msec=(tempoimpiegato.tv_sec*1000)+(tempoimpiegato.tv_usec/1000);
	/*
	fprintf(	f, "tempoimpiegato: %lu : %ld sec %ld usec\n\n", 
			id, tempoimpiegato.tv_sec, tempoimpiegato.tv_usec );
	*/
	return(msec);
}

void save_delay(FILE *f, uint32_t *buf)
{
	struct timeval sent, now, tempoimpiegato;
	uint32_t id;
	long int msec;

	if(f==NULL) return;
	id=buf[0];
	memcpy( (char*)&sent, (char*)&(buf[1]), sizeof(struct timeval) );
	gettimeofday(&now,NULL);
	/*
	fprintf( f,	"\nsent: %lu : %ld sec %ld usec\n", id, sent.tv_sec, sent.tv_usec );
	fprintf( f,	"now:  %lu : %ld sec %ld usec\n", id, now.tv_sec, now.tv_usec );
	*/
	tempoimpiegato=differenza(now,sent);
	msec=(tempoimpiegato.tv_sec*1000)+(tempoimpiegato.tv_usec/1000);
	/*
	fprintf(	f, "tempoimpiegato: %lu : %ld sec %ld usec\n\n", 
			id, tempoimpiegato.tv_sec, tempoimpiegato.tv_usec );
	*/
	if(msec>150)
	{
		numritardi++;
		printf("%u : delay msec %ld  TEMPO SUPERATO\n", id, msec);
	}
	else
		printf("%u : delay msec %ld\n", id, msec);
	fprintf(f,"%u %ld\n", id, msec);
	fflush(f);

	/*
	if(msec>150)
	{
		fprintf( f,	"\nsent: %lu : %ld sec %ld usec\n", id, sent.tv_sec, sent.tv_usec );
		fprintf( f,	"now:  %lu : %ld sec %ld usec\n", id, now.tv_sec, now.tv_usec );
		fprintf(	f, "tempoimpiegato: %lu : %ld sec %ld usec\n\n", 
				id, tempoimpiegato.tv_sec, tempoimpiegato.tv_usec );
		fprintf(f, "TEMPO SUPERATO - TERMINO\n");
		fflush(f);
		exit(888);
	}
	*/
}


int send_udp(uint32_t socketfd, char *buf, uint32_t len, uint16_t port_number_local, char *IPaddr, uint16_t port_number_dest)
{
	int ris;
	struct sockaddr_in To;
	int addr_size;

	/* assign our destination address */
	memset( (char*)&To,0,sizeof(To));
	To.sin_family		=	AF_INET;
	To.sin_addr.s_addr  =	inet_addr(IPaddr);
	To.sin_port			=	htons(port_number_dest);

	addr_size = sizeof(struct sockaddr_in);
	/* send to the address */
	ris = sendto(socketfd, buf, len , MSG_NOSIGNAL, (struct sockaddr*)&To, addr_size);
	if (ris < 0) {
		printf ("sendto() failed, Error: %d \"%s\"\n", errno,strerror(errno));
		return(0);
	}
	return(1);
}



#define PARAMETRIDEFAULT "./Appmobile.exe 6001 2"
void usage(void) 
{	printf ( "usage:  ./Appmobile.exe REMOTEPORT TIPOSPEDIZIONE\n");
	printf ( "                                0=NOPKT 1=TUTTIPKT 2=VOCEESILENZI\n"
	         "esempio: "  PARAMETRIDEFAULT "\n");
}

int main(int argc, char *argv[])
{
	uint16_t portLBmobile;
	int ris, LBmobilefd;
	pthread_t th;
	int primoricevuto=0;
	int tipogenerazionepkt;
	pkt_generator_parameters params;

	if(argc==1) { 
		printf ("uso i parametri di default \n%s\n", PARAMETRIDEFAULT );
		portLBmobile = 6001;
		tipogenerazionepkt=2;
	}
	else if(argc!=3) { printf ("necessari 2 parametri\n"); usage(); exit(1);  }
	else { /* leggo parametri da linea di comando */
		portLBmobile = atoi(argv[1]);
		tipogenerazionepkt=atoi(argv[2]);
	}

	if ((signal (SIGHUP, sig_close)) == SIG_ERR) { perror("signal (SIGHUP) failed: "); EExit(2); }
	if ((signal (SIGINT, sig_close)) == SIG_ERR) { perror("signal (SIGINT) failed: "); EExit(2); }
	if ((signal (SIGTERM, sig_close)) == SIG_ERR) { perror("signal (SIGTERM) failed: "); EExit(2); }

	init_random();
	ris=socketpair(AF_UNIX,SOCK_STREAM,0,fds);
	if (ris < 0) {	perror("socketpair fds0 failed: ");	EExit(1); }
	/*
	ris=SetsockoptTCPNODELAY(fds[0],1); if (!ris)  EExit(5);
	ris=SetsockoptTCPNODELAY(fds[1],1); if (!ris)  EExit(5);
	*/

	/* mi connetto al LBmobile */
	ris=TCP_setup_connection(&LBmobilefd, "127.0.0.1", portLBmobile,  300000, 300000, 1);
	if(!ris) {	printf ("TCP_setup_connection() failed\n"); EExit(1); }
	f=fopen("delaymobile.txt","wt");
	if(f==NULL) { perror("fopen failed"); EExit(1); }

	/* inizializzo il sistema di controllo dei pkt ricevuti e/o duplicati */ 
	init_checkrecvMobile();
	init_checkrecvMobileDelay();
	
	FD_ZERO(&all);
	FD_SET(LBmobilefd,&all);
	maxfd=LBmobilefd;
	FD_SET(fds[0],&all);
	if(maxfd<fds[0]) maxfd=fds[0];

	/* faccio partire il pthread pkt_generator per attivare la generazione di pacchetti */
	params.tipogenerazionepkt=tipogenerazionepkt;
	params.fd=fds[1];
	ris = pthread_create (&th, NULL, pkt_generator, (void*)&params );
	if (ris){
		printf("ERROR; return code from pthread_create() is %d\n",ris);
		EExit(-1);
	}

	for(;;)
	{
		struct timeval timeout;
		long int msecdelay;

		do {
			rdset=all;
			timeout.tv_sec=10;
			timeout.tv_usec=0;
			ris=select(maxfd+1,&rdset,NULL,NULL,&timeout);
			/* ris=select(maxfd+1,&rdset,NULL,NULL,&timeout); */
		} while( (ris<0) && (errno==EINTR) );
		if(ris<0) {
			perror("select failed: ");
			EExit(1);
		}

		/* se arriva qualcosa dalla connessione TCP con LBmobile, leggo!!!! */
		if( FD_ISSET(LBmobilefd,&rdset) )
		{
			uint32_t buf[PKTSIZE], idletto; int ris;

#ifdef VICDEBUG
			fprintf(stderr,"in arrivo qualcosa dalla connessione TCP del LBmobile:\n");
#endif
			/* ris=recv(LBmobilefd,(char*)buf,PKTSIZE,MSG_DONTWAIT); */
			/*ris=Readn(LBmobilefd,(char*)buf,PKTSIZE);*/
			ris=Readn(LBmobilefd,(char*)buf,sizeof(buf));
			/*if(ris!=PKTSIZE) { fprintf(stderr,"recv from LBmobile failed, received %d: ", ris); EExit(9); }*/
			if(ris!=sizeof(buf)) { fprintf(stderr,"recv from LBmobile failed, received %d: ", ris); EExit(9); }
			idletto=buf[0];
			/* printf("ricevuto pkt id %u\n",idletto); */
			msecdelay=compute_delay(buf);
			if( check_pkt_recv_at_Mobile(idletto) == 1 ) /* pacchetto duplicato */
			{
				printf("ricevuto pkt duplicato id %d delay %ld msec \n",idletto, msecdelay);
				numduplicati++;
			}
			else
			{
				/* memorizzo di avere ricevuto il pkt */
				set_pkt_recv_at_Mobile(idletto);
				SetpktrecvMobileDelay(idletto,msecdelay);
#ifdef OUTPUT_MEDIO
				printf("ricevuto pkt id %lu delay %ld msec \n",idletto, msecdelay);
#endif
				if(primoricevuto==0) {
					primoricevuto=1;
					idlastrecv=idletto;
				}
				else if(idletto>idlastrecv) {
					idlastrecv=idletto;
				}
				if(msecdelay>150) {
					numritardi++;
					printf("%d : delay msec %ld  TEMPO SUPERATO\n", idletto, msecdelay);
				}
				else {
					numok++;
					printf("%d : delay msec %ld\n", idletto, msecdelay);
				}
				/* save_delay(f,buf); */
			}
		}
		else
		{
			if( FD_ISSET(fds[0],&rdset) )
			{
				char ch;
				uint32_t buf[PKTSIZE];
				struct timeval sent;

				do {
					ris=recv(fds[0],&ch,1,0);
				} while( (ris<0) && (errno==EINTR) );
				if(ris<0) {
					perror("Appmobile - recv from pkt_generator failed: ");
					sleep(1);
					EExit(1);
				}
				/* spedisco i pkt */
				/*memset((char*)buf,0,PKTSIZE);*/
				memset((char*)buf,0,sizeof(buf));
				buf[0]=idmsg;
				gettimeofday(&sent,NULL);
				memcpy( (char*)&(buf[1]), (char*)&sent, sizeof(struct timeval) );

				/*
				 * fprintf(stderr,"pkt %u sent %ld sec %ld usec\n", idmsg, sent.tv_sec, sent.tv_usec ); 
				 */

				/*ris=Sendn(LBmobilefd, (char*)buf, PKTSIZE  );*/
				ris=Sendn(LBmobilefd, (char*)buf, sizeof(buf) );
				/*if(ris!=PKTSIZE) {*/
				if(ris!=sizeof(buf)) {
					fprintf(stderr,"Appmobile - Sendn failed   ris %d  TERMINO\n", ris);
					sleep(1);
					EExit(1);
				}
#ifdef OUTPUT_MEDIO
				gettimeofday(&sent,NULL);
				fprintf(stderr,"pkt %u sent %dB %ld sec %ld usec\n", idmsg, ris, sent.tv_sec, sent.tv_usec );
#endif
				idmsg++;
			}
		}

	} /* fine for ;; */
	return(0);
}

