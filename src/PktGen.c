/* PktGen.c  */

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

#define __PKTGEN_C__

#include "../include/Util.h"
#include "../include/VoIPpkt.h"
#include "../include/PktGen.h"

/* #define VICDEBUG */

struct timeval istfinenuovostato(int nuovostato)
{
	struct timeval finestato, lenstato;

	gettimeofday(&finestato,NULL);
	normalizza(&finestato);
	lenstato.tv_sec=0;
	lenstato.tv_usec=	
		USEC_VOIPPKTLEN
		+
		(
			(
				( long int )
				(
					( random() %(MAX_LENGTH_TALKSPURT_OR_SILENCE_USEC-USEC_VOIPPKTLEN) )
					/
					USEC_VOIPPKTLEN
				)
			)
			*
			USEC_VOIPPKTLEN
		);
	/* fprintf(stderr,"lenstato %ld sec %ld usec\n", lenstato.tv_sec, lenstato.tv_usec); */
	somma(finestato,lenstato,&finestato);
	return(finestato);
}

void *pkt_generator(void *p)
{
	pkt_generator_parameters *pparams;
	struct timeval timeout; char ch=1; int ris, tipogenerazionepkt;
	int fd;
	int stato=1; /* parla */
	struct timeval finestato;
	struct timeval now;

	if(p==NULL) {
		fprintf(stderr,"puntatore nullo - termino\n");
		exit(1);
	}
	pparams=(pkt_generator_parameters*)p;
	tipogenerazionepkt=pparams->tipogenerazionepkt;
	fd=pparams->fd;

	if(tipogenerazionepkt==2) { 
		finestato=istfinenuovostato(stato);
		fprintf(stderr,"finestato %ld sec %ld usec\n", finestato.tv_sec, finestato.tv_usec);
	}
	for(;;)
	{
		do {
			/* */
			gettimeofday(&now,NULL);
			switch(tipogenerazionepkt) 
			{
				case 2: /* silenzio e parlato */
					if( scaduto_timeout( &finestato )   ) {
						/* cambio stato */
						if(stato==1)	{
							stato=0;	/* da adesso silenzio */
							fprintf(stderr, ROSSO "inizio silenzio" DEFAULTCOLOR "\n");
						}
						else	{
							stato=1;	/* parla */
							fprintf(stderr,ROSSO "inizio talkspurt" DEFAULTCOLOR "\n");
						}
						finestato=istfinenuovostato(stato);
					}
					/* se parlo, aspetto 40 msec e poi genero pacchetto */
					if(stato==1)	{
						timeout.tv_sec=0;
						timeout.tv_usec=USEC_VOIPPKTLEN;  /* 40000 */
					}
					else { /* se non parlo aspetto scadenza finestato */
						timeout=differenza(finestato,now);
					}
					break;

				case 0: /* quasi muto */
					timeout.tv_sec=10; /* 10 sec */
					timeout.tv_usec=0;
					break;

				case 1: /* chiacchierone */
				default:
					timeout.tv_sec=0;
					timeout.tv_usec=USEC_VOIPPKTLEN;  /* 40000 */
					break;
			}
			normalizza(&timeout);
			/* printf("timeout %ld sec %ld usec\n", timeout.tv_sec, timeout.tv_usec ); */
			ris=select(1,NULL,NULL,NULL,&timeout);
		} while( 
					((ris<0) && (errno==EINTR))
					||					
					( scaduto_timeout( &finestato )==0   )
				);
		if(ris<0) {
			perror("thread scheduler - select failed: TERMINO ");
			sleep(1);
			exit(1);
		} else if(ris==0) {
			;
		}
		do {
			ris=send(fd,&ch,1, MSG_NOSIGNAL);
		} while( (ris<0)&&(errno==EINTR));
		if(ris<0) {
			perror("thread scheduler - select failed: TERMINO ");
			sleep(1);
			exit(1);
		}
	}
	pthread_exit(NULL);
	return(NULL);
}



