/* Monitor.c  */

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
#include <assert.h>
#include <math.h>

#include "../include/Util.h"
#include "../include/VoIPpkt.h"

extern long int random(void);
extern void srandom(unsigned int seed);


/* #define VICDEBUG */
/* #define OUTPUT_MEDIO */

/* per permettere di chiudere porta quando premo enter 
#define CHIUSURAPORTE_MANUALE
*/

#define P(X) do { fprintf(stderr,X "\n"); fflush(stderr); } while(0)

typedef struct {
	int32_t fd_latomobile;
	uint16_t port_number_latomobile;
	int32_t fd_latofixed;
	uint16_t port_number_latofixed;
	int attivo;
	long int sec_istcreazione;
	int stato_trasmissione; /* 0 perdere, 1 spedire */
	int latenzamedia_msec;
	struct timeval ist_prossimo_cambio_stato;
} COPPIAFD;
static COPPIAFD coppiafd[MAXNUMCONNECTIONS];

#define CMD_ADDPORT	1
#define CMD_SEND	2
#define CMD_NOTIFY 3
#define CMD_SETLENTA 4
#define CMD_SETOK 5
#define CMD_REMOVELOSS 6

struct structelementolista;
typedef struct structelementolista{
	int cmd;
	/* usati solo se cmd==CMD_SEND */
	int32_t fd;
	uint16_t port_number_local;
	uint16_t port_number_dest;
	uint32_t len;
	char *buf;
	/* usati solo se cmd==CMD_NOTIFY */
	char tipoack;
	uint32_t idmsg;
	/* definiscono istante d'uso dell'elemento e prossimo elemento */
	struct timeval timeout;
	struct structelementolista *next;

} ELEMENTOLISTA;
static ELEMENTOLISTA *root;

typedef struct {
	char tipoack;
	uint32_t id;
}  __attribute__((packed)) ACK;


#define LATENZAMEDIANORMALE_MSEC 70
#define LATENZAMEDIATROPPOGRANDE_MSEC 200

/* scarto almeno 5% di pkt nel tratto wireless, senza burst */
#define PERCENTUALEERROREMINIMAWIRELESS9 9
/* scarto circa 1% di pkt nel tratto wired, senza avvisare il mobile */
#define PERCENTUALEERROREMINIMAWIRED1 1 
#define PERC_ERR 15   /* 15% */
#define RITARDO_NOTIFICA_ACKNACK_USEC 30000L /* TRENTA MILLISECONDI */
#define INTERVALSETLENTA_SEC 10 /* DIECI SECONDI */

/* definizione per change */
#define MINBURSTLEN_MSEC 200
#define MAXBURSTLEN_MSEC 800
#define MINSENDINGLEN_MSEC 1000
#define MAXSENDINGLEN_MSEC 8000
/* definizione per change lenta */
#define MINCHANGELENTALEN_MSEC 5000
#define MAXCHANGELENTALEN_MSEC 8000
/* definizione per remove */
#define MINREMOVELEN_MSEC 10000
#define MAXREMOVELEN_MSEC 15000

#define PORTNUMBER_RANGESIZE 100

static int counter_localport_mobileside=0, counter_localport_fixedside=0;
static uint16_t	first_local_port_number_mobile_side, first_local_port_number_fixed_side;
static fd_set rdset, all;
static int listening_monitorfd=-1, monitorfd=-1;
static int maxfd;
static int bytespeditiM2F=0;
static int numspeditiM2F=0;
static int numscartatiM2F=0;
static int numarrivatiatiM2F=0;
static int bytespeditiF2M=0;
static int numspeditiF2M=0;
static int numscartatiF2M=0;
static int numarrivatiatiF2M=0;
static int printed=0;

static void close_coppia(int i);

static void sig_print(int signo)
{
	int i;
	if(printed==0)
	{
		printed=1;
		for(i=0;i<MAXNUMCONNECTIONS;i++)
			close_coppia(i);

		if(listening_monitorfd>=0)
		{
			FD_CLR(listening_monitorfd,&all);
			close(listening_monitorfd);
			listening_monitorfd=-1;
		}
		if(monitorfd>=0)
		{
			FD_CLR(monitorfd,&all);
			close(monitorfd);
			monitorfd=-1;
		}

		if(signo==SIGINT)		printf("SIGINT\n");
		else if(signo==SIGHUP)	printf("SIGHUP\n");
		else if(signo==SIGTERM)	printf("SIGTERM\n");
		else					printf("other signo\n");
		printf("\n");

		printf("M2F bytespediti %d numspediti %d  numscartati %d numarrivati %d \n",
					bytespeditiM2F, numspeditiM2F, numscartatiM2F, numarrivatiatiM2F );
		printf("F2M bytespediti %d numspediti %d  numscartati %d numarrivati %d \n",
					bytespeditiF2M, numspeditiF2M, numscartatiF2M, numarrivatiatiF2M );
		fflush(stdout);
	}
	exit(0);
	return;
}

static void Exit(int errcode)
{
	sig_print(0);
}

#if 0
static void stampa_fd_set(char *str, fd_set *pset)
{
	int i;
	printf("%s ",str);
	for(i=0;i<100;i++) if(FD_ISSET(i,pset)) printf("%d ", i);
;	printf("\n");
	fflush(stdout);
}
#endif

static int get_local_port(int socketfd)
{
	int ris;
	struct sockaddr_in Local;
	unsigned int addr_size;

	addr_size=sizeof(Local);
	memset( (char*)&Local,0,sizeof(Local));
	Local.sin_family		=	AF_INET;
	ris=getsockname(socketfd,(struct sockaddr*)&Local,&addr_size);
	if(ris<0) { perror("getsockname() failed: "); return(0); }
	else {
		/*
		fprintf(stderr,"IP %s port %d\n", inet_ntoa(Local.sin_addr), ntohs(Local.sin_port) );
		*/
		return( ntohs(Local.sin_port) );
	}
}

static int	send_configurazione(int monitorfd)
{
	int i, ris; uint32_t num, sent=0;
	char ch='C';

	ris=Sendn(monitorfd,&ch,sizeof(ch));
	if(ris!=sizeof(ch)) { fprintf(stderr,"send_configurazione failed: "); Exit(9); }
	sent+=ris;

	for(i=0,num=0;i<MAXNUMCONNECTIONS;i++)	if(coppiafd[i].attivo==1) num++;
#ifdef VICDEBUG
	fprintf(stderr,"send_configurazione: num attivi %d\n", num);
#endif
	ris=Sendn(monitorfd,&num,sizeof(num));
	if(ris!=sizeof(num)) { fprintf(stderr,"send_configurazione failed: "); Exit(9); }
	sent+=ris;

	for(i=0;i<MAXNUMCONNECTIONS;i++) {
		if(coppiafd[i].attivo==1) {
#ifdef VICDEBUG
			fprintf(stderr,"send_configurazione: porta %d\n", coppiafd[i].port_number_latomobile );
#endif
			ris=Sendn(monitorfd,&(coppiafd[i].port_number_latomobile),sizeof(coppiafd[i].port_number_latomobile));
			if(ris!=sizeof(coppiafd[i].port_number_latomobile)) { fprintf(stderr,"send_configurazione failed: "); Exit(9); }
			sent+=ris;
		}
	}
	return(sent);	
}

static int check_port(uint16_t port_number_local)
{
	int i;
	
	for(i=0;i<MAXNUMCONNECTIONS;i++)	
	{
		if( (coppiafd[i].attivo==1)  &&
			(
				(
					(coppiafd[i].port_number_latomobile==port_number_local) &&
					(coppiafd[i].fd_latomobile>0) &&
					(coppiafd[i].fd_latofixed>0)
				)

				||
				(
					(coppiafd[i].port_number_latofixed==port_number_local) &&
					(coppiafd[i].fd_latofixed>0) &&
					(coppiafd[i].fd_latomobile>0)
				)
			)
		  )
		  return(1);
	}
	return(0);
}

static void stampa_lista(void)
{
#ifdef VICDEBUG
	char str[1024];
	int i=0;
	ELEMENTOLISTA *p=root;
	
	while(p!=NULL) {
		switch(p->cmd) {
			case CMD_ADDPORT:
				strncpy(str,"ADDPORT",1024);
			break;
			case CMD_SEND:
				strncpy(str,"SEND",1024);
			break;
			case CMD_NOTIFY:
				strncpy(str,"NOTIFY",1024);
			break;
			case CMD_SETLENTA:
				strncpy(str,"SETLENTA",1024);
			break;

			case CMD_SETOK:
				strncpy(str,"SETOK",1024);
			break;
			case CMD_REMOVELOSS:
				strncpy(str,"REMOVELOSS",1024);
			break;
			default:
				strncpy(str,"UNKNOWN",1024);
		}
		fprintf(stderr,"%d %s timeout %ld sec %ld usec\n", i,
									str, p->timeout.tv_sec, p->timeout.tv_usec );
		p = p->next;
		i++;
	}
#endif
}

static void aggiungi_in_ordine(ELEMENTOLISTA *p)
{
	ELEMENTOLISTA* *pp=&root;

	if(p==NULL) 
		return;
	while(*pp!=NULL) {
		if( minore( &(p->timeout) , &( (*pp)->timeout) ) ) {
			ELEMENTOLISTA *tmp;
			tmp=*pp;
			*pp=p;
			p->next=tmp;
			tmp=NULL;
			return;
		}
		else
			pp = &( (*pp)->next );
	}
	if(*pp==NULL) {
		*pp=p;
		p->next=NULL;
	}
	stampa_lista();
}

static void free_pkt(ELEMENTOLISTA* *proot)
{
	ELEMENTOLISTA *tmp;

	if(proot) {
		if(*proot) {
			tmp=*proot;
			*proot = (*proot)->next;
			if(tmp->buf) {
				free(tmp->buf);
				tmp->buf=NULL;
			}
			free(tmp);
		}
	}
	stampa_lista();
}

static void close_coppia(int i)
{
	if(coppiafd[i].attivo==1)
	{
		coppiafd[i].attivo=0;
		if(coppiafd[i].fd_latomobile>=0) 
		{
			FD_CLR(coppiafd[i].fd_latomobile,&all);
			close(coppiafd[i].fd_latomobile);
			coppiafd[i].fd_latomobile=-1;
		}
		if(coppiafd[i].fd_latofixed>=0)
		{
			FD_CLR(coppiafd[i].fd_latofixed,&all);
			close(coppiafd[i].fd_latofixed);
			coppiafd[i].fd_latofixed=-1;
		}
	}
}

static int ricevo_inserisco(	int i, uint32_t *pidmsg, uint32_t fd_latoricevere, uint32_t fd_latospedire,
					 						uint16_t port_number_latospedire, uint16_t port_number_dest,
											struct timeval *pritardo_aggiunto, int *pnumbytes )
{
	/* leggo, calcolo ritardo e metto in lista da spedire */
	char buf[65536];
	struct sockaddr_in From;
	unsigned int Fromlen; int ris;

	/* wait for datagram */
	do {
		memset(&From,0,sizeof(From));
		Fromlen=sizeof(struct sockaddr_in);
		ris = recvfrom ( fd_latoricevere, buf, (int)65536, 0, (struct sockaddr*)&From, &Fromlen);
	} while( (ris<0) && (errno==EINTR) );
	if (ris<0) {
		if(errno==ECONNRESET) {
			perror("recvfrom() failed (ECONNRESET): ");
			fprintf(stderr,"ma non chiudo il socket\n");
			fflush(stderr);
		}
		else {
			perror("recvfrom() failed: "); fflush(stderr);
			close_coppia(i);
		}
		return(0);
	}
	else if(ris>0) 
	{
		*pnumbytes=ris;
		if(ris>=sizeof(*pidmsg))
			memcpy( (char*)pidmsg,buf, sizeof(*pidmsg) );
		else {
			memset((char*)pidmsg,0,sizeof(*pidmsg));
			memcpy( (char*)pidmsg,buf, ris );
		}

#ifdef OUTPUT_MEDIO
		{
			struct timeval t, creato;	gettimeofday(&t,NULL);
			memcpy(&creato,buf+4,sizeof(creato));
			fprintf(stderr,"ricevuto  pkt id %u da port %d - sec %ld usec %ld creato %ld sec %ld usec\n", 
										*pidmsg, get_local_port(fd_latoricevere),
										t.tv_sec, t.tv_usec,
										creato.tv_sec, creato.tv_usec
										);
		}
#endif

		/* decido se spedire o scartare */
		if( coppiafd[i].stato_trasmissione==0) {
			/* 	rete wireless scarta causa 
				burst di perdite di durata tra 100 ms e 500 ms
				intervallati da sequenze di spediti tra 500 msec e 1500 sec
			*/
			return(1);
		}
		else
		{
			int casuale;
			struct timeval now;

			gettimeofday(&now,NULL);
			casuale=random()%100 - 3*abs ( sin( (now.tv_sec-coppiafd[i].sec_istcreazione)/8.0 ) );
			/* printf("CASUALE %d\n", casuale); */

			/* modifica */
			if(casuale<=PERCENTUALEERROREMINIMAWIRELESS9  /*PERCENTUALE_ERRORE*/) { 
				/* scarto almeno il 9% dei pkt indipendentemente dai burst */
				return(1);
			}
			else if( casuale <= 
							 (PERCENTUALEERROREMINIMAWIRELESS9+PERCENTUALEERROREMINIMAWIRED1)
						) { /* rete wired scarta 1% dei pkt, ma wireless avvisa con ACK */
				return(3);
			}
			else {
				/* alloco spazio per il pacchetto dati */
				ELEMENTOLISTA *p;
				struct timeval delay;

				p=(ELEMENTOLISTA *)malloc(sizeof(ELEMENTOLISTA));
				if(p==NULL) { perror("malloc failed: "); Exit(9); }
				p->buf=(char*)malloc(ris);
				if(p->buf==NULL) { perror("malloc failed: "); Exit(9); }

				p->cmd=CMD_SEND;
				p->len=ris; /* dimensione del pacchetto */
				p->fd=fd_latospedire;
				p->port_number_local=port_number_latospedire;
				p->port_number_dest=port_number_dest;
				/* copio il messaggio ricevuto */
				memcpy(p->buf,buf,ris);
				/* calcolo il ritardo da aggiungere e calcolo il timeout */
				delay.tv_sec=0;
				/* tra 50 e 140 ms */
				delay.tv_usec=
								coppiafd[i].latenzamedia_msec*1000 /* BASE del canale */
								+ (random()%20000) /* VARIAZIONE CASUALE */
								+ abs(10000*sin( (now.tv_sec-coppiafd[i].sec_istcreazione)/8.0 )); /* ANDAMENTO */
				/* printf("DELAY %d\n", delay.tv_usec); */
				/* normalizza(&delay); */
				*pritardo_aggiunto=delay;
	
				gettimeofday(&(p->timeout),NULL);
				somma(p->timeout,delay,&(p->timeout));
#if 0
					{
						fprintf(stderr,"calcolato ritardo  pkt id %u ritardo_aggiunto sec %ld usec %ld\n", 
										*pidmsg, delay.tv_sec, delay.tv_usec);
						fprintf(stderr,"messo il lista  pkt id %u timeout sec %ld usec %ld\n", 
										*pidmsg, p->timeout.tv_sec, p->timeout.tv_usec);
					}
#endif

				/* metto in lista */
				aggiungi_in_ordine(p);
				p=NULL;
				return(2);
			}
		}

	} /* fine ris>0 */
	return(0);
}

static void schedula_change_ok(char *msg)
{
	/* alloco spazio per il pacchetto dati */
	ELEMENTOLISTA *p;
	struct timeval delay;

	p=(ELEMENTOLISTA *)malloc(sizeof(ELEMENTOLISTA));
	if(p==NULL) { perror("malloc failed: "); Exit(9); }
	p->buf=NULL;
	p->cmd=CMD_SETOK;
	/* calcolo il ritardo da aggiungere e calcolo il timeout */
	delay.tv_sec=0; 
	delay.tv_usec=	1000* 
								(
									MINSENDINGLEN_MSEC +
									( random() % (MAXSENDINGLEN_MSEC-MINSENDINGLEN_MSEC) )
								);
	normalizza(&delay);
#ifdef VICDEBUG
	fprintf(stderr,"timeout changeok %ld sec\n", delay.tv_sec );
#endif
	gettimeofday(&(p->timeout),NULL);
	somma(p->timeout,delay,&(p->timeout));
	/* metto in lista */
	aggiungi_in_ordine(p);
	p=NULL;
}

static void schedula_change_lenta(char *msg)
{
	/* alloco spazio per il pacchetto dati */
	ELEMENTOLISTA *p;
	struct timeval delay;

#ifdef VICDEBUG
	struct timeval now;
	gettimeofday(&now,NULL);
	if(msg==NULL)
		fprintf(stderr,"+schedula_change_lenta now %ld sec %ld usec\n", now.tv_sec, now.tv_usec );
	else
		fprintf(stderr,"%s schedula_change_lenta now %ld sec %ld usec\n", msg, now.tv_sec, now.tv_usec );
#endif

	p=(ELEMENTOLISTA *)malloc(sizeof(ELEMENTOLISTA));
	if(p==NULL) { perror("malloc failed: "); Exit(9); }
	p->buf=NULL;
	p->cmd=CMD_SETLENTA;
	/* calcolo il ritardo da aggiungere e calcolo il timeout */
	delay.tv_sec=0; 
	delay.tv_usec= 1000*
					(
						MINCHANGELENTALEN_MSEC +
						( random() % (MAXCHANGELENTALEN_MSEC-MINCHANGELENTALEN_MSEC) )
					);
	normalizza(&delay);
#ifdef VICDEBUG
	fprintf(stderr,"timeout changelenta %ld sec\n", delay.tv_sec );
#endif
	gettimeofday(&(p->timeout),NULL);
	somma(p->timeout,delay,&(p->timeout));
	/* metto in lista */
	aggiungi_in_ordine(p);
	p=NULL;
}

static void schedula_remove_loss(char *msg)
{
	/* alloco spazio per il pacchetto dati */
	ELEMENTOLISTA *p;
	struct timeval delay;

	p=(ELEMENTOLISTA *)malloc(sizeof(ELEMENTOLISTA));
	if(p==NULL) { perror("malloc failed: "); Exit(9); }
	p->buf=NULL;
	p->cmd=CMD_REMOVELOSS;
	/* calcolo il ritardo da aggiungere e calcolo il timeout */
	delay.tv_sec=0; 
	delay.tv_usec=	1000* 
								(
									MINREMOVELEN_MSEC +
									( random() % (MAXREMOVELEN_MSEC-MINREMOVELEN_MSEC)  )
								);
	normalizza(&delay);
#ifdef VICDEBUG
	fprintf(stderr,"timeout removeloss %ld sec\n", delay.tv_sec );
#endif
	gettimeofday(&(p->timeout),NULL);
	somma(p->timeout,delay,&(p->timeout));
	/* metto in lista */
	aggiungi_in_ordine(p);
	p=NULL;
}

static void schedula_creazione_nuova_porta(void)
{
	/* alloco spazio per il pacchetto dati */
	ELEMENTOLISTA *p;
	struct timeval delay;

	p=(ELEMENTOLISTA *)malloc(sizeof(ELEMENTOLISTA));
	if(p==NULL) { perror("malloc failed: "); Exit(9); }
	p->buf=NULL;
	p->cmd=CMD_ADDPORT;
	/* calcolo il ritardo da aggiungere e calcolo il timeout */
	delay.tv_sec=2;
	delay.tv_usec=500000; /* due secondi e mezzo */
	gettimeofday(&(p->timeout),NULL);
	somma(p->timeout,delay,&(p->timeout));
	/* metto in lista */
	aggiungi_in_ordine(p);
	p=NULL;
}

/* crea porta che all'inizio non trasmette */
static void creazione_nuova_coppia_porte(int sec_prossimo_cambio_stato_trasmissione)
{
	int i;

	for(i=0;i<MAXNUMCONNECTIONS;i++)
	{
		if(coppiafd[i].attivo==0) /* vuoto */
		{
			/* cerco coppia di porte non usata */
			int trovate=0, j;

			for(j=0;j<PORTNUMBER_RANGESIZE;j++)
			{
				int localport_mobileside, localport_fixedside, ris1, ris2;

				localport_mobileside=	
								first_local_port_number_mobile_side+
								( (counter_localport_mobileside+j)%PORTNUMBER_RANGESIZE );
				localport_fixedside=	
								first_local_port_number_fixed_side+
								( (counter_localport_fixedside+j)%PORTNUMBER_RANGESIZE );
				fprintf(stderr,"trying UDP port %d %d \n", localport_mobileside,localport_fixedside);

				ris1=UDP_setup_socket_bound( 	&(coppiafd[i].fd_latomobile), 
																			localport_mobileside, 65535, 65535 );
				if (!ris1)
					fprintf(stderr,"UDP_setup_socket_bound() mobile failed\n");

				ris2=UDP_setup_socket_bound( 	&(coppiafd[i].fd_latofixed), 
																			localport_fixedside, 65535, 65535 );
				if (!ris2)
					fprintf(stderr,"UDP_setup_socket_bound() fixed failed\n");

				if( ris1 && ris2 ) 
				{ /* entrambe le porte settate */

					struct timeval now;

					trovate=1;
					coppiafd[i].port_number_latomobile=localport_mobileside;
					counter_localport_mobileside+=j+1;
					coppiafd[i].port_number_latofixed=localport_fixedside;
					counter_localport_fixedside+=j+1;

					coppiafd[i].attivo=1;
					FD_SET(coppiafd[i].fd_latomobile,&all);
					if(coppiafd[i].fd_latomobile>maxfd)
						maxfd=coppiafd[i].fd_latomobile;
					FD_SET(coppiafd[i].fd_latofixed,&all);
					if(coppiafd[i].fd_latofixed>maxfd)
						maxfd=coppiafd[i].fd_latofixed;
					gettimeofday(&now,NULL);
					normalizza(&now);
					coppiafd[i].sec_istcreazione=now.tv_sec;

					/* modifica per burst di perdite */
					coppiafd[i].stato_trasmissione=0; /* 0 perdere, 1 spedire */
					coppiafd[i].latenzamedia_msec=LATENZAMEDIANORMALE_MSEC;
					coppiafd[i].ist_prossimo_cambio_stato=now;
					coppiafd[i].ist_prossimo_cambio_stato.tv_sec += sec_prossimo_cambio_stato_trasmissione;
					/* fine modifica per burst di perdite */

					return; /* entrambi i socket settati, termino */
				}
				else 
				{	/* almeno un socket non settato, devo rilasciare i settati */
					if(ris1) {
						if(coppiafd[i].fd_latomobile>=0) {
							close(coppiafd[i].fd_latomobile);
							coppiafd[i].fd_latomobile=-1;
						}
					}
					if(ris2) {
						if(coppiafd[i].fd_latofixed>=0) {
							close(coppiafd[i].fd_latofixed);
							coppiafd[i].fd_latofixed=-1;
						}
					}
					/* continuo il loop */
				}
			} /* fine for j */

			/* se arrivo qui vuol dire che non ho trovato porte libere */
			fprintf(stderr,"unable to setup UDP socket.  Quit\n");
			Exit(1);

		} /* fine if attivo */
	} /* fine for i */
}

#ifdef CHIUSURAPORTE_MANUALE
static int conta_attive_no_lente_no_burst(void)
{
	int i, count=0;

	for(i=0;i<MAXNUMCONNECTIONS;i++)
	{
		if(coppiafd[i].attivo==1)
		{
			if(coppiafd[i].latenzamedia_msec==LATENZAMEDIANORMALE_MSEC)
			{
				if(coppiafd[i].stato_trasmissione==1) /* spedisce */
				{
					count++;
				}
			}
		}
	}
	return(count);
}

static int close_coppia_no_lenta_no_burst(void)
{
	int i;

	for(i=0;i<MAXNUMCONNECTIONS;i++)
	{
		if(coppiafd[i].attivo==1)
		{
			if(coppiafd[i].latenzamedia_msec==LATENZAMEDIANORMALE_MSEC)
			{
				if(coppiafd[i].stato_trasmissione==1) /* spedisce */
				{
					fprintf(stderr,"chiusa coppia di porte no lenta no burst\n");
					close_coppia(i);
					return(1);
				}
			}
		}
	}
	return(0);
}

static int conta_attive_no_lente_burst(void)
{
	int i, count=0;

	for(i=0;i<MAXNUMCONNECTIONS;i++)
	{
		if(coppiafd[i].attivo==1)
		{
			if(coppiafd[i].latenzamedia_msec==LATENZAMEDIANORMALE_MSEC)
			{
				if(coppiafd[i].stato_trasmissione==0) /* scarto */
				{
					count++;
				}
			}
		}
	}
	return(count);
}
				
static int close_coppia_no_lenta_burst(void)
{
	int i;

	for(i=0;i<MAXNUMCONNECTIONS;i++)
	{
		if(coppiafd[i].attivo==1)
		{
			if(coppiafd[i].latenzamedia_msec==LATENZAMEDIANORMALE_MSEC)
			{
				if(coppiafd[i].stato_trasmissione==0) /* scarto */
				{
					fprintf(stderr,"chiusa coppia di porte no lente burst\n");
					close_coppia(i);
					return(1);
				}
			}
		}
	}
	return(0);
}

static int conta_attive_lente(void)
{
	int i, count=0;

	for(i=0;i<MAXNUMCONNECTIONS;i++)
	{
		if(coppiafd[i].attivo==1)
		{
			if(coppiafd[i].latenzamedia_msec==LATENZAMEDIATROPPOGRANDE_MSEC)
			{
				count++;
			}
		}
	}
	return(count);
}

static int close_coppia_lenta(void)
{
	int i;

	for(i=0;i<MAXNUMCONNECTIONS;i++)
	{
		if(coppiafd[i].attivo==1)
		{
			if(coppiafd[i].latenzamedia_msec==LATENZAMEDIATROPPOGRANDE_MSEC)
			{
				fprintf(stderr,"chiusa coppia di porte lente\n");
				close_coppia(i);
				return(1);
			}
		}
	}
	return(0);
}
#endif  /* CHIUSURAPORTE_MANUALE */

static int init_1ok_1lenta_2burst(void)
{
	int i0,j;

	i0=random()%MAXNUMCONNECTIONS;

	j=(i0+0)%MAXNUMCONNECTIONS;
	coppiafd[j].latenzamedia_msec=LATENZAMEDIATROPPOGRANDE_MSEC;
	coppiafd[j].stato_trasmissione=1;
		
	j=(i0+1)%MAXNUMCONNECTIONS;
	coppiafd[j].latenzamedia_msec=LATENZAMEDIANORMALE_MSEC;
	coppiafd[j].stato_trasmissione=0;
		
	j=(i0+2)%MAXNUMCONNECTIONS;
	coppiafd[j].latenzamedia_msec=LATENZAMEDIANORMALE_MSEC;
	coppiafd[j].stato_trasmissione=0;
		
	j=(i0+3)%MAXNUMCONNECTIONS;
	coppiafd[j].latenzamedia_msec=LATENZAMEDIANORMALE_MSEC;
	coppiafd[j].stato_trasmissione=1;

	return(1);
}


static int check_1ok_1burst(void)
{
	int i, nok=0, nlente=0, nburst=0;

	for(i=0;i<MAXNUMCONNECTIONS;i++)
	{
		if(	coppiafd[i].attivo==1 )
		{
			if(coppiafd[i].latenzamedia_msec==LATENZAMEDIATROPPOGRANDE_MSEC) nlente++;
			else { /* latenza normale */
				if(coppiafd[i].stato_trasmissione==1) nok++;
				else nburst++;
			}
		}
	}
	if( (nok>=1) && (nburst>=1) ) return(1);
	else return(0);
}

/* sia i l'indice della porta lenta, allora
 * faccio diventare lenta la porta di indice i+1
 * successiva alla attuale lenta, 
 * se la i+1 era burst la vecchia lenta diventa burst,
 * se la i+1 era ok la vecchia lenta diventa ok.
 *
 * se invece la lenta non c'era, faccio diventare lenta
 * la prima porta burst che incontro
 */
static int change_lenta(void)
{
	int i,i0,j,trovata=0,trasmissione;

	/* cerco lenta */
	for(i0=0;i0<MAXNUMCONNECTIONS;i0++)
	{
		if(	(coppiafd[i0].attivo==1) &&
				(coppiafd[i0].latenzamedia_msec==LATENZAMEDIATROPPOGRANDE_MSEC)
			)
		{
			trovata=1;
			break;
		}
	}
	if(trovata)
	{
		/* cerco la successiva alla lenta */
		for(i=1;i<MAXNUMCONNECTIONS;i++)
		{
			j= (i0+i) % MAXNUMCONNECTIONS;
			if( coppiafd[j].attivo==1 )
			{
				trasmissione=coppiafd[j].stato_trasmissione;
				/* setto la porta successiva come nuova lenta */
				coppiafd[j].latenzamedia_msec=LATENZAMEDIATROPPOGRANDE_MSEC;
				coppiafd[j].stato_trasmissione=coppiafd[i0].stato_trasmissione;
				/* setto la vecchia lenta come normale */
				coppiafd[i0].stato_trasmissione=trasmissione;
				coppiafd[i0].latenzamedia_msec=LATENZAMEDIANORMALE_MSEC;
				return(1);
			}
		}
		return(0);
	}
	else /* nessuna lenta */
	{
		for(i=0;i<MAXNUMCONNECTIONS;i++)
		{
			if(	(coppiafd[i].attivo==1) &&
					(coppiafd[i].stato_trasmissione==0)
				)
			{
				coppiafd[i].latenzamedia_msec=LATENZAMEDIATROPPOGRANDE_MSEC;
				return(1);
			}
		}
		return(0);
	}
	return(0);
}


static int change_ok(void)
{
	int i,i0,j,trovata=0;

	/* cerco ok */
	for(i0=0;i0<MAXNUMCONNECTIONS;i0++)
	{
		if(	(coppiafd[i0].attivo==1) &&
				(coppiafd[i0].latenzamedia_msec==LATENZAMEDIANORMALE_MSEC) &&
				(coppiafd[i0].stato_trasmissione==1)
			)
		{
			trovata=1;
			break;
		}
	}
	if(trovata)
	{
		/* cerco la successiva */
		for(i=1;i<MAXNUMCONNECTIONS;i++)
		{
			j= (i0+i) % MAXNUMCONNECTIONS;
			if( (coppiafd[j].attivo==1 ) &&
					(coppiafd[j].stato_trasmissione==0)
				)
			{
				if( coppiafd[j].latenzamedia_msec==LATENZAMEDIATROPPOGRANDE_MSEC)
				{
					coppiafd[j].stato_trasmissione=1;
					/* ma lascio la vecchia ok ancora ok */
				}
				else
				{
					coppiafd[j].stato_trasmissione=1;
					coppiafd[i0].stato_trasmissione=0;
				}
				return(1);
			}
		}
	}
	return(0);
}


/* int close_coppia_no_lenta_burst(void) */
static int remove_loss(void)
{
	int i;

	for(i=0;i<MAXNUMCONNECTIONS;i++)
	{
		if(coppiafd[i].attivo==1)
		{
			if(coppiafd[i].latenzamedia_msec==LATENZAMEDIANORMALE_MSEC)
			{
				if(coppiafd[i].stato_trasmissione==0) /* scarto */
				{
					fprintf(stderr,"chiusa coppia di porte no lente burst\n");
					close_coppia(i);
					return(1);
				}
			}
		}
	}
	return(0);
}

static void stampa_coppie_porte(void)
{
	int i;

	for(i=0;i<MAXNUMCONNECTIONS;i++)
	{
		if(coppiafd[i].attivo==1)
		{

			fprintf(stderr,"(%d)", get_local_port(coppiafd[i].fd_latomobile) ); 
			if(coppiafd[i].latenzamedia_msec==LATENZAMEDIATROPPOGRANDE_MSEC) 
			{
				if(coppiafd[i].stato_trasmissione==1) 
					fprintf(stderr,"LENTA     "); 
				else
					fprintf(stderr,"LENTALOSS "); 
			}
			else
			{
				if(coppiafd[i].stato_trasmissione==1) 
					fprintf(stderr,"OK        ");
				else
					fprintf(stderr,"LOSS      ");
			}
		}
	}
	fprintf(stderr,"\n");
	
#ifdef VICDEBUG
	for(i=0;i<MAXNUMCONNECTIONS;i++)
	{
		if(coppiafd[i].attivo==1)
		{
			fprintf(stderr,"coppia %d: latomobile fd %d port %d - latofixed fd %d port %d \n",
					i, coppiafd[i].fd_latomobile, get_local_port(coppiafd[i].fd_latomobile), 
					coppiafd[i].fd_latofixed, get_local_port(coppiafd[i].fd_latofixed) );
		}
	}
#endif
}

static struct timeval compute_timeout_first_pkt(void)
{
	struct timeval now, attesa;

	gettimeofday(&now,NULL);
	attesa=differenza(root->timeout,now);
	/* incremento la attesa di 1 msec */
	attesa.tv_usec+=1000;
	normalizza(&attesa);
	return(attesa);
}

static int send_udp(uint32_t socketfd, char *buf, uint32_t len, uint16_t port_number_local, char *IPaddr, uint16_t port_number_dest)
{
	int ris;
	struct sockaddr_in To;
	int addr_size;

	/* assign our destination address */
	memset( (char*)&To,0,sizeof(To));
	To.sin_family		=	AF_INET;
	To.sin_addr.s_addr  =	inet_addr(IPaddr);
	To.sin_port			=	htons(port_number_dest);

#ifdef VICDEBUG
	fprintf(stderr,"send_udp sending to %d\n", port_number_dest);
#endif

	addr_size = sizeof(struct sockaddr_in);
	/* send to the address */
	ris = sendto(socketfd, buf, len , MSG_NOSIGNAL, (struct sockaddr*)&To, addr_size);
	if (ris < 0) {
		printf ("sendto() failed, Error: %d \"%s\"\n", errno,strerror(errno));
		return(0);
	}
	return(1);
}

/* schedula notifica spedito/scartato dopo 30 msec */
static int schedula_notify( char tipoack, uint32_t idmsg)
{
	ELEMENTOLISTA *p;
	struct timeval delay;

	/* controllo che cosa devo notificare */
	if( (tipoack!='A') && (tipoack!='N') ) {
		fprintf(stderr,
						"schedula_notify: tipo di notifica %c (%d) non ammesso. Termino\n",
						tipoack, (int)tipoack ); 
		Exit(97); 
	}

	/* alloco spazio per il pacchetto di notifica */
	p=(ELEMENTOLISTA *)malloc(sizeof(ELEMENTOLISTA));
	if(p==NULL) { perror("malloc failed: "); Exit(9); }

	p->buf=NULL;
	p->cmd=CMD_NOTIFY;
	p->tipoack=tipoack;
	p->idmsg=idmsg;
	/* assegno il ritardo di notifica da aggiungere e calcolo il timeout */
	delay.tv_sec=0;
	delay.tv_usec=RITARDO_NOTIFICA_ACKNACK_USEC; /* TRENTA MILLISECONDI */
	gettimeofday(&(p->timeout),NULL);
	somma(p->timeout,delay,&(p->timeout));
	/* metto in lista */
	aggiungi_in_ordine(p);
	p=NULL;
	return(1);
}


static void notify(int monitorfd, char cmdack, uint32_t idmsg)
{
	ACK ack; int ris;
	
	if(cmdack=='A')
		ack.tipoack='A';
	else if(cmdack=='N')
		ack.tipoack='N';
	else {
		fprintf(stderr,"tipo di notifica %c (%d) non ammesso. Termino\n",
										cmdack, (int)cmdack ); 
		Exit(98); 
	}
	memcpy( (char*)&(ack.id), (char*)&idmsg, sizeof(uint32_t) );
	ris=Sendn(monitorfd,(char*)&ack,sizeof(ACK));
	if(ris!=sizeof(ACK)) { 
		fprintf(stderr,"notify failed: "); 
		Exit(99); 
	}
}

#define PARAMETRIDEFAULT "./Monitor.exe 7001 8000 8001 9001 10001 1"
static void usage(void) 
{  printf ("usage: ./Monitor.exe REMOTEPORTMOBILE LOCALPORTMONITOR FIRSTLOCALPORTMOBILESIDE FIRSTLOCALPORTFIXEDSIDE REMOTEPORTFIXED CFG_AUTO\n"
				"esempio:   PARAMETRIDEFAULT\n"
				"CFG_AUTO==1 abilita rconfigurazione automatica delle porte"
				"CFG_AUTO==0 abilita la sola configurazione manuale (premere ENTER) delle porte"
				);
}

#if 0
static int prova_gen_casuale(void)
{
	/* leggo, calcolo ritardo e metto in lista da spedire */
	int i;
	int casuale;
	struct timeval creaz, now;

	gettimeofday(&creaz,NULL);
	for(i=0;;i++) {

		{
			struct timeval t;
			t.tv_sec=0;
			t.tv_usec=USEC_VOIPPKTLEN;  /* 40000 */
			normalizza(&t);
			select(0,NULL,NULL,NULL,&t);
		}
		gettimeofday(&now,NULL);
		casuale=random()%100 - 3*abs ( sin( (now.tv_sec-creaz.tv_sec)/8.0 ) );
		/* printf("CASUALE %d\n", casuale); */

		/* modifica */
		if(casuale<PERCENTUALEERROREMINIMAWIRELESS9  /*PERCENTUALE_ERRORE*/) { 
			/* scarto almeno il 9% dei pkt indipendentemente dai burst */
			printf("pkt %d soggetto a SCARTO WIRELESS di BASE %d\n", i, casuale);
		}
		else if( casuale <= 
						 (PERCENTUALEERROREMINIMAWIRELESS9+PERCENTUALEERROREMINIMAWIRED1)
					) { /* rete wired scarta 1% dei pkt, ma wireless avvisa con ACK */
			printf("pkt %d soggetto a SCARTO WIRED %d\n", i, casuale);
		}
		else {
			struct timeval delay;

			/* calcolo il ritardo da aggiungere e calcolo il timeout */
			delay.tv_sec=0;
			/* tra 50 e 140 ms */
			delay.tv_usec=
#if 0
							LATENZAMEDIANORMALE_MSEC*1000 /* BASE del canale */
							+ (random()%20000) /* VARIAZIONE CASUALE */
							+ 
#endif

							abs(20000*sin( (now.tv_sec-creaz.tv_sec)/8.0 )); /* ANDAMENTO */
			printf("ANDAMENTO %d DELAY %ld msec\n", i, delay.tv_usec/1000);
			/*
			gettimeofday(&(p->timeout),NULL);
			somma(p->timeout,delay,&(p->timeout));
			*/
		}
	}
	return(0);
}
#endif

int main(int argc, char *argv[])
{
	uint16_t	local_port_number_monitor,
				remote_port_number_mobile, remote_port_number_fixed;
	struct sockaddr_in Cli;
	unsigned int len;
	int modificata_cfg=0, ordine_chiusura_canale=0, riconfigurazione_automatica;
	int i, ris;

	if(argc==1) { 
		printf ("uso i parametri di default \n%s\n", PARAMETRIDEFAULT);
		remote_port_number_mobile = 7001;
		local_port_number_monitor = 8000;
		first_local_port_number_mobile_side = 8001;
		first_local_port_number_fixed_side = 9001;
		remote_port_number_fixed = 10001;
		riconfigurazione_automatica=1;
	}
	else if(argc!=7) { printf ("necessari 6 parametri\n"); usage(); Exit(1);  }
	else { /* leggo parametri da linea di comando */
		remote_port_number_mobile = atoi(argv[1]);
		local_port_number_monitor = atoi(argv[2]);
		first_local_port_number_mobile_side = atoi(argv[3]);
		first_local_port_number_fixed_side = atoi(argv[4]);
		remote_port_number_fixed = atoi(argv[5]);
		riconfigurazione_automatica=atoi(argv[6]);
		if( riconfigurazione_automatica!=1 &&	riconfigurazione_automatica!=0 )
			{ printf ("parametro CFG_AUTO errato\n"); usage(); Exit(1);  }
	}
	for(i=0;i<MAXNUMCONNECTIONS;i++) {
		coppiafd[i].fd_latomobile=-1;
		coppiafd[i].fd_latofixed=-1;
		coppiafd[i].attivo=0;
	}
	root=NULL;
	init_random();

	/*
	prova_gen_casuale();
	return(0);
	*/


	if ((signal (SIGHUP, sig_print)) == SIG_ERR) { perror("signal (SIGHUP) failed: "); Exit(2); }
	if ((signal (SIGINT, sig_print)) == SIG_ERR) { perror("signal (SIGINT) failed: "); Exit(2); }
	if ((signal (SIGTERM, sig_print)) == SIG_ERR) { perror("signal (SIGTERM) failed: "); Exit(2); }
	ris=TCP_setup_socket_listening( &listening_monitorfd, local_port_number_monitor, 300000, 300000, 1);
	if (!ris)
	{	printf ("TCP_setup_socket_listening() failed\n");
		Exit(1);
	}
#ifdef VICDEBUG
	fprintf(stderr,"socket listening %d\n", listening_monitorfd);
	fflush(stderr);
#endif

	/* attendo la connessione dal lato mobile */
	do { memset (&Cli, 0, sizeof (Cli));		
		len = sizeof (Cli);
		monitorfd = accept ( listening_monitorfd, (struct sockaddr *) &Cli, &len);
	} while ( (monitorfd<0) && (errno==EINTR) );
	if (monitorfd < 0 ) {	
		perror("accept() failed: \n");
		Exit (1);
	}
	ris=SetsockoptTCPNODELAY(monitorfd,1); if (!ris) { fprintf(stderr,"TCPNODELAY failed\n");  Exit(5); }
	/* chiusura del socket TCP listening e setup dei socket UDP	*/
	close(listening_monitorfd);
	FD_CLR(listening_monitorfd,&all);
	listening_monitorfd=-1;
#ifdef VICDEBUG
	fprintf(stderr,"socket monitorfd %d\n", monitorfd);
	fflush(stderr);
#endif

	FD_ZERO(&all);

	FD_SET(monitorfd,&all);
	maxfd=monitorfd;

	for(i=0;i<MAXNUMCONNECTIONS;i++) 
		/* una perde dopo 5 sec, l'altra dopo 10 sec */
		creazione_nuova_coppia_porte(i*5); 

	init_1ok_1lenta_2burst();
	schedula_change_ok("main inizio");
	schedula_change_lenta("main inizio");

	if( riconfigurazione_automatica==1 )
		schedula_remove_loss("main inizio");
	else {
		/* stdin */
		FD_SET(0,&all);
	}

/* #ifdef VICDEBUG */
	stampa_coppie_porte();
/* #endif */

	ris=send_configurazione(monitorfd);
#ifdef VICDEBUG
	fprintf(stderr,"send_configurazione sent %d\n", ris);
#endif

	for(;;)
	{
		modificata_cfg=0;
		ordine_chiusura_canale=0;

/* ripeti: ; */
		do {
			int myerrno;

			rdset=all;
#ifdef VICDEBUG
			stampa_fd_set("rdset prima",&rdset);
#endif
			if(root!=NULL) {
				struct timeval timeout;
				timeout=compute_timeout_first_pkt();
#ifdef VICDEBUG
				fprintf(stderr,"set timeout %d sec %d usec\n", timeout.tv_sec,timeout.tv_usec );
#endif
				ris=select(maxfd+1,&rdset,NULL,NULL,&timeout);
				myerrno=errno;
			}
			else {
#ifdef VICDEBUG
				fprintf(stderr,"set timeout infinito\n");
#endif
				ris=select(maxfd+1,&rdset,NULL,NULL,NULL);
				myerrno=errno;
			errno=myerrno;
			}
		} while( (ris<0) && (errno==EINTR) && (printed==0) );
		if(ris<0) {
			perror("select failed: ");
			Exit(1);
		}
		if(printed!=0) {
			fprintf(stderr,"esco da select dopo avere gia' segnalato la chiusura, TERMINO\n");
			Exit(1);
		}
#ifdef VICDEBUG
		if(ris==0) {
			fprintf(stderr, "select: timeout expired !\n");
		}
#endif
#ifdef VICDEBUG
		stampa_fd_set("rdset dopo",&rdset);
#endif

		/* se arriva qualcosa dalla connessione TCP con lato mobile, termino!!!! */
		if(	FD_ISSET(monitorfd,&rdset)  )
		{
			char buf[1000]; int ris;
			fprintf(stderr,"in arrivo qualcosa dalla connessione TCP del mobile: che e'?\n");
			fflush(stderr);
			/* ris=recv(monitorfd,buf,1000,MSG_DONTWAIT); */
			ris=read(monitorfd,buf,1000);
			if(ris<0) {
				if(errno==EINTR) { fprintf(stderr,"recv monitorfd EINTR, continuo\n"); break; }
				else if(errno==ECONNRESET) { perror("recv monitorfd failed: ECONNRESET "); fprintf(stderr,"NON TERMINO\n"); }
				else  {	int myerrno=errno; perror("recv monitorfd failed: "); fprintf(stderr,"errno %d\n",myerrno); 
						fprintf(stderr,"NON TERMINO\n"); Exit(11); 
				}
			}
			else if(ris==0) { fprintf(stderr,"recv ZERO from monitorfd: TERMINO\n"); Exit(12); }
			else {
				int j;
				fprintf(stderr,"recv %d from monitorfd: ", ris);
				for(j=0;j<ris;j++) fprintf(stderr,"%c",buf[j]);
				fprintf(stderr,"\n");
			}
			break; /* esco dal for piu' esterno */
		}

		/* se arriva qualcosa dallo stdin, chiudo una coppia di porte 
		   e schedulo la creazione di una nuova porta
		*/
		if(	FD_ISSET(0,&rdset)  )
		{
			char *ptr=NULL; char str[256];

			ptr=fgets(str,512,stdin);
			if(ptr!=NULL) {
				fprintf(stderr,"letto da stdin - elimino una porta\n");
				fflush(stderr);

				ris=remove_loss();
				if(ris==0)	fprintf(stderr,"unable to remove the channel\n"); 
				modificata_cfg=1;
				stampa_coppie_porte();
				send_configurazione(monitorfd);
				schedula_creazione_nuova_porta();
				continue; /* ricomincio il loop */
			}
		}

		/* spedisco o elimino i pkt da spedire */
		while( (root!=NULL) && scaduto_timeout( &(root->timeout) )   ) 
		{
			if(root->cmd==CMD_SEND) {
				if( check_port(root->port_number_local) ) {
					struct timeval t;
					gettimeofday(&t,NULL);
					ris=send_udp(root->fd,root->buf,root->len,root->port_number_local,"127.0.0.1",root->port_number_dest);
#ifdef OUTPUT_MEDIO
					fprintf(stderr,"pkt id %u sent %d from %d to %d  at %ld sec %ld usec\n", 
									*((uint32_t*)(root->buf)), ris, root->port_number_local, root->port_number_dest,
									t.tv_sec, t.tv_usec );
#endif
				}
				free_pkt(&root);
			}
			else if(root->cmd==CMD_ADDPORT) {
				/* inserisco una nuova coppia di porte burst */
				creazione_nuova_coppia_porte( 0 );
				/* setto almeno una porta no burst, una porta lenta e una porta burst */
				stampa_coppie_porte();
				if( riconfigurazione_automatica==1 )
						schedula_remove_loss("root->cmd==CMD_ADDPORT");
				send_configurazione(monitorfd);
				modificata_cfg=1;
				free_pkt(&root);
			}
			else if(root->cmd==CMD_NOTIFY) {
				switch(root->tipoack) {
					case 'A':
						#ifdef VICDEBUG
						fprintf(stderr,"notifico SPEDITO idmsg %u\n", root->idmsg);
						#endif
						notify(monitorfd,'A',root->idmsg); /* notifica spedito */
					break;

					case 'N':
						#ifdef VICDEBUG
						fprintf(stderr,"notifico SCARTO idmsg %u\n", root->idmsg);
						#endif
						notify(monitorfd,'N',root->idmsg); /* notifica scartato */
					break;

					default:;
						fprintf(stderr,"tipo notifica %c NON VALIDA idmsg %u\n", 
														root->tipoack, root->idmsg );
				}
				free_pkt(&root);
			}
			else if(root->cmd==CMD_SETLENTA) {
				ris=change_lenta();
				if(ris==0)	fprintf(stderr,"unable to change lenta\n"); 
				modificata_cfg=1;
				stampa_coppie_porte();
				schedula_change_lenta("root->cmd==CMD_SETLENTA");
				free_pkt(&root);
			}
			else if(root->cmd==CMD_SETOK) {
				ris=change_ok();
				if(ris==0)	fprintf(stderr,"unable to change the ok\n"); 
				modificata_cfg=1;
				stampa_coppie_porte();
				schedula_change_ok("root->cmd==CMD_SETOK");
				free_pkt(&root);
			}
			else if(root->cmd==CMD_REMOVELOSS) {
				ris=remove_loss();
				if(ris==0)	fprintf(stderr,"unable to remove the channel\n"); 
				modificata_cfg=1;
				stampa_coppie_porte();
				send_configurazione(monitorfd);
				schedula_creazione_nuova_porta();
				free_pkt(&root);
			}
			else {
				free_pkt(&root);
			}
		}

		/* verifico che ci sia almeno una veloce ok ed una veloce burst 
		 */
		if( (modificata_cfg==1) && (check_1ok_1burst()==0) )
		{
			fprintf(stderr,"non abbastanza porte adatte alla trasmissione:  Exit\n");
			stampa_coppie_porte();
			Exit(333);
		}

		/* gestione pacchetti in arrivo */
		if( modificata_cfg==0 )
		{
			int numbytes;
			for(i=0;i<MAXNUMCONNECTIONS;i++)	
			{
				uint32_t idmsg;
				if(coppiafd[i].attivo==1)
				{
					if( FD_ISSET(coppiafd[i].fd_latomobile,&rdset) )
					{
						struct timeval ritardo_aggiunto;
	
						#ifdef VICDEBUG
						fprintf(stderr,"leggo da lato mobile\n");
						#endif
						/* leggo, calcolo ritardo e metto in lista da spedire verso il fixed */
						ris=ricevo_inserisco(	i, &idmsg, coppiafd[i].fd_latomobile, coppiafd[i].fd_latofixed,
												coppiafd[i].port_number_latofixed, remote_port_number_fixed,
												&ritardo_aggiunto, &numbytes );
						if(ris==0)		{ 
							/* non ricevuto niente, o errore sul socket */
							P("NULLAmobile");  
							; 
						}	/* non ricevuto niente, o errore sul socket */
						else if(ris==1)	{ 
#ifdef OUTPUT_MEDIO
							fprintf(stderr,"deciso SCARTO idmsg %u da latomobile\n", idmsg);
#endif
							bytespeditiM2F+=numbytes;
							numspeditiM2F++;
							numscartatiM2F++;
							schedula_notify('N',idmsg); /* schedula notifica scartato */
						}
						else if(ris==2)	{ 
#ifdef OUTPUT_MEDIO
							fprintf(stderr,"deciso SPEDISCO idmsg %u da latomobile ritardoaggiunto %ld msec\n", idmsg,
															ritardo_aggiunto.tv_sec*1000+(ritardo_aggiunto.tv_usec/1000)   );
#endif
							bytespeditiM2F+=numbytes;
							numspeditiM2F++;
							numarrivatiatiM2F++;
							schedula_notify('A',idmsg); /* schedula notifica spedito */
						}
						else if(ris==3)	{ 
#ifdef OUTPUT_MEDIO
							fprintf(stderr,"deciso PERSOWIRED idmsg %u da latomobile\n", idmsg);
#endif
							bytespeditiM2F+=numbytes;
							numspeditiM2F++;
							numscartatiM2F++;
							schedula_notify('A',idmsg); /* schedula notifica spedito */
						}
						else { P("CHEE'mobile "); ;}
					}
				}

				if(coppiafd[i].attivo==1)
				{
					if( FD_ISSET(coppiafd[i].fd_latofixed,&rdset) )
					{
						struct timeval ritardo_aggiunto;

						#ifdef VICDEBUG
						fprintf(stderr,"leggo da lato fixed\n");
						#endif
						/* leggo, calcolo ritardo e metto in lista da spedire */
						ris=ricevo_inserisco(	i, &idmsg, coppiafd[i].fd_latofixed, coppiafd[i].fd_latomobile, 
												coppiafd[i].port_number_latomobile, remote_port_number_mobile,
												&ritardo_aggiunto, &numbytes );
						if(ris==0)		{ P("NULLAfixed");  ; }	/* non ricevuto niente, o errore sul socket */
						else if(ris==1)	{ 
#ifdef OUTPUT_MEDIO
							fprintf(stderr,"SCARTO idmsg %u da latofixed\n", idmsg);
#endif
							bytespeditiF2M+=numbytes;
							numspeditiF2M++;
							numscartatiF2M++;
							/* notify(monitorfd,0,idmsg); NO notifica scartato */
							;
						}
						else if(ris==2)	{ 
							/* P("SPEDISCOdafixed");  */
#ifdef OUTPUT_MEDIO
							fprintf(stderr,"deciso SPEDISCOdafixed idmsg %u da latomobile ritardoaggiunto %ld msec\n", idmsg,
															ritardo_aggiunto.tv_sec*1000+(ritardo_aggiunto.tv_usec/1000)   );
#endif
							bytespeditiF2M+=numbytes;
							numspeditiF2M++;
							numarrivatiatiF2M++;
							/* notify(monitorfd,1,idmsg); NO notifica spedito */
							;
						}
						else if(ris==3)	{ 
#ifdef OUTPUT_MEDIO
							P("PERDOWIREDdafixed"); 
#endif
							bytespeditiF2M+=numbytes;
							numspeditiF2M++;
							numscartatiF2M++;
							/* notify(monitorfd,1,idmsg); NO notifica spedito */
							;
						}
						else { P("CHEE'fixed "); ;}
					}

				}
			} /* fine for i */
		}

	} /* fine for ;; */

	for(i=0;i<MAXNUMCONNECTIONS;i++)
		if(coppiafd[i].attivo==1)
			close_coppia(i);
	return(0);
}

