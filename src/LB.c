/**
 *  @file LB.c
 *  @author Vincenzo Ferrari - Barbara Iadarola - Luca Giuliano
 *  @brief Collezione di funzioni
 *  @note Questo file contiene le funzioni usate da entrambi i Load Balancer ripetutamente.
 */

#define __LB_C__

#include "../include/LB.h"

extern int printed;
extern int numspediti;
extern int numscartati;

void sig_print(int signo)
{
	if(printed==0)
	{
		printed=1;
		if(signo==SIGINT)		printf("SIGINT\n");
		else if(signo==SIGHUP)	printf("SIGHUP\n");
		else if(signo==SIGTERM)	printf("SIGTERM\n");
		else					printf("other signo\n");
		printf("\n");

		printf("numspediti %d  numscartati %d\n", numspediti, numscartati );
		fflush(stdout);
	}
	exit(0);
	return;
}

void stampa_fd_set(char *str, fd_set *pset)
{
	int i;
	printf("%s ",str);
	for(i=0;i<100;i++) 
		if(FD_ISSET(i,pset)) 
			printf("%d ", i);
	printf("\n");
}

int send_udp(uint32_t socketfd, char *buf, uint32_t len, uint16_t port_number_local, char *IPaddr, uint16_t port_number_dest)
{
	int ris;
	struct sockaddr_in To;
	int addr_size;

	/* assign our destination address */
	memset( (char*)&To,0,sizeof(To));
	To.sin_family		= AF_INET;
	To.sin_addr.s_addr	= inet_addr(IPaddr);
	To.sin_port		= htons(port_number_dest);

	addr_size = sizeof(struct sockaddr_in);
	/* send to the address */
	ris = sendto(socketfd, buf, len , MSG_NOSIGNAL, (struct sockaddr*)&To, addr_size);
	if (ris < 0) {
		printf ("sendto() failed, Error: %d \"%s\"\n", errno,strerror(errno));
		return(0);
	}
	return(ris);
}

long int compute_delay(uint32_t *buf)
{
	struct timeval sent, now, tempoimpiegato;
	uint32_t idmsg;
	long int msec;

	idmsg=buf[0];
	memcpy( (char*)&sent, (char*)&(buf[1]), sizeof(struct timeval) );
	gettimeofday(&now,NULL);

	tempoimpiegato=differenza(now,sent);
	msec=(tempoimpiegato.tv_sec*1000)+(tempoimpiegato.tv_usec/1000);

	return(msec);
}

/**
  * @brief Gestisce l'invio dei PING su tutte le porte attive.
  * @param numporte : numero delle porte attive
  * @param *cfgPorte : puntatore al vettore delle porte
  * @param monitordatafd : file descriptor del Monitor
  * @return void.
 */
void send_ping(uint32_t numporte, uint16_t *cfgPorte, int32_t monitordatafd)
{
	uint32_t pingPKT[2];
	struct timeval timePkt;
	int i, ris;
	
	for(i=0; i<numporte; i++)
	{
		memset((char*)pingPKT,0,sizeof(pingPKT));
				
		gettimeofday(&timePkt,NULL);
		memcpy( (char*)&(pingPKT[0]), (char*)&timePkt, sizeof(struct timeval) );
			
		ris=send_udp(monitordatafd, (char*)pingPKT, sizeof(pingPKT) , 1000, "127.0.0.1", cfgPorte[i] );
		if(!(ris))
		{
			fprintf(stderr,"PING non inviato\n");
		}
	}
}

/**
  * @brief Gestisce l'aggiornamento dell'indice del vettore di porte.
  * @param *buf : puntatore al pacchetto di configurazione
  * @param numporte : numero di porte attive
  * @param *cfgPorte : puntatore al vettore di porte
  * @param index : indice del vettore
  * @param portaAttuale : attuale porta d'invio
  * @return Restituisce il nuovo indice oppure cicla il vettore di porte.
 */
int config_porte(uint32_t *buf, uint32_t numporte, uint16_t *cfgPorte, int index, uint16_t portaAttuale)
{
	int i;

	/* Cerco l'indice della porta attuale nel vettore di porte aggiornato */
	/* Se non è presente, 'i' diventa 5 */
	i=0;
	while((cfgPorte[i] != portaAttuale) && (i<numporte))
	{
		i++;
	}
	if(i < (numporte)) return (i);
	/* Altrimenti l'aggiorno all'attuale indice */
	else return ((index + 1)%numporte);
}

/**
  * @brief Gestisce l'invio del pacchetto di configurazione delle porte.
  * @param *cfgPKT : puntatore al pacchetto di configurazione
  * @param monitordatafd : file descriptor del Monitor
  * @param *cfgPorte : puntatore al vettore di porte
  * @param numporte : numero di porte attive
  * @return void.
 */
void send_config(uint32_t *cfgPKT, int32_t monitordatafd, uint16_t *cfgPorte, uint32_t numporte)
{
	struct timeval timePkt;
	int i, risCFG;
	
	for(i=0; i<numporte; i++)
	{				
		/* Imprime il timestamp sul pacchetto */
		gettimeofday(&timePkt,NULL);
		memcpy( (char*)&(cfgPKT[1]), (char*)&timePkt, sizeof(struct timeval) );

		/* Invia della nuova configurazione delle porte al Fixed */
		risCFG=send_udp(monitordatafd, (char*)cfgPKT, 32 , 1000, "127.0.0.1", cfgPorte[i]);
		if(!(risCFG))
 		{
			fprintf(stderr,"Errore durante l'invio della nuova configurazione!\n");
		}
	}
}

/**
  * @brief Aggiorna l'indice del vettore di porte.
  * @param portFrom : porta da cui proviene il pacchetto di configurazione
  * @param *cfgPorte : puntatore al vettore di porte
  * @param index : indice del vettore di porte
  * @param numporte : numero delle porte attive
  * @return Restituisce l'indice aggiornato oppure quello corrente.
 */
int trova_porta(uint16_t portFrom, uint16_t *cfgPorte, int index, uint32_t numporte)
{
	int i;
	
	/* Salvo la porta del pacchetto appena giunto dal Mobile */
	i=0;
	while((cfgPorte[i] != portFrom) && (i<numporte))
	{
		i++;
	}
	if(i < numporte) return(i);
	else return(index);
}

/**
  * @brief Inizializza il vettore passato per parametro.
  * @param *vettore : puntatore al vettore
  * @return void.
 */
void initV(uint32_t *vettore)
{
	memset(&vettore,0,sizeof(vettore));
}
