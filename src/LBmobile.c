/**
 *  @file LBmobile.c
 *  @author Vincenzo Ferrari - Barbara Iadarola - Luca Giuliano
 *  @brief Gestione della connessione Wireless
 *  @note Questo file consente la gestione della connessione Wireless tra l'Applicazione Mobile e il Monitor
 */

#include "../include/LBliste.h"
#include "../include/LB.h"

/* #define VICDEBUG */

#define TIPO_CONN

#define P(X) do { fprintf(stderr,X "\n"); fflush(stderr); } while(0)

/* Vettore di porte */
uint16_t cfgPorte[MAXNUMCONNECTIONS];

fd_set rdset, all;
int maxfd;
int numspediti=0;
int numscartati=0;
int printed=0;

/* Porte */
uint16_t portmonitor, localportfrommobile, localportdatamonitor;
uint32_t numporte;

/* Vettore di porte */
uint16_t porte[MAXNUMCONNECTIONS];
struct sockaddr_in Cli;
unsigned int len;
int32_t monitordatafd;

/* FD */
int listening_mobilefd, appmobilefd, monitorfd;

/* Contatori, variabili di supporto */
int ris, i, k;

/* Variabile per il tipo di connessione attivata */
uint32_t tipoCnt;

/* Variabili per la gestione dei cronometri e del delay */
struct timeval timePkt, sent, now, tempoimpiegato;

/* Contatore degli ACK per gestire il 9% di pacchetti persi su porta 'OK' */
int contACK;

/* Indice del vettore delle porte */
int index;

/* Controllo per l'invio della prima configurazione delle porte */
int risCFG;

/* Pacchetto della configurazione delle porte */
uint32_t cfgPKT[SIZE_BUF_CFGPKT];

/* Liste di pacchetti */
struct listaPKT *lPkt;

/* Timestamp per il controllo del ritardo dei primi pacchetti per il controllo del tipo di connessione */
struct timeval timePktFirst;

/* Vettore per il timestamp del tipo di connessione */
uint32_t tipoConn[SIZE_TIPOCONN];

/* Pacchetto del tipo di connessione */
uint32_t pktTipoConn[SIZE_TIPOCONN];

/**
  * @brief Gestisce la prima configurazione delle porte proveniente dal Monitor.
  * @param monitorfd : file descriptor del monitor
  * @param *pnumporte : puntatore al numero delle porte
  * @param porte[] : vettore di porte
  * @return Ritorna un intero.
 */
int	RecvCfg(int monitorfd, uint32_t *pnumporte, uint16_t porte[] )
{
	char ch; int i, ris;
	
	fprintf(stderr,"RecvCfg starts\n");
	/* ris=recv(monitorfd,buf,65536,MSG_DONTWAIT); */
	ris=recv(monitorfd,&ch,1,0);
	if(ris!=1) { fprintf(stderr,"RecvCfg failed: "); exit(9); }
	printf("ricevo dal monitor: %c\n", ch);
	fflush(stdout);
	if(ch=='C') {
		ris=recv(monitorfd,pnumporte,sizeof(uint32_t),0);
		if(ris!=sizeof(uint32_t)) { fprintf(stderr,"RecvCfg recv_uint32_t cfgPKT[PKTSIZE];configurazione failed: "); exit(9); }
		printf("ricevo dal monitor: num porte %u\n", *pnumporte );
		fflush(stdout);
		for(i=0;i<*pnumporte;i++) {
			ris=recv(monitorfd,&(porte[i]),sizeof(uint16_t),0);
			if(ris!=sizeof(uint16_t)) { fprintf(stderr,"RecvCfg recv_configurazione failed: "); exit(9); }
			fprintf(stderr,"ricevo dal monitor: porta %d\n", (uint16_t)porte[i] );
			fflush(stderr);
		}
		fprintf(stderr,"RecvCfg terminated correctly\n");
		fflush(stderr);
		return(1);	
	}
	fprintf(stderr,"RecvCfg receive wrong char: failed: ");
	exit(9);
	return(0);	
}

/**
  * @brief Aggiorna il vettore di porte.
  * @param *pnumporte : puntatore al numero di porte
  * @param *vporte : puntatore al vettore di porte
  * @param *pnewnumporte : puntatore al nuovo numero di porte
  * @param *newvporte : puntatore al nuovo vettore di porte
  * @return void.
 */
void setup_new_configurazione(uint32_t *pnumporte, uint16_t *vporte, uint32_t *pnewnumporte, uint16_t *newvporte)
{
	int i;
	/* copio la nuova configurazione */
	*pnumporte=*pnewnumporte;
	for(i=0;i<*pnumporte;i++) {
		cfgPorte[i]=vporte[i]=newvporte[i];

	}
	for(i=*pnumporte;i<MAXNUMCONNECTIONS;i++) {
		vporte[i]=0;
	}
}

/**
  * @brief Calcola il ritardo dei pacchetti PING ricevuti dal Load Balancer Fixed.
  * @param *buf : puntatore al pacchetto PING
  * @return Restituisce il ritardo del pacchetto espresso in millisecondi.
 */
long int compute_delay_ping(uint32_t *buf)
{
	struct timeval sent, now, tempoimpiegato;
	long int msec;

	memcpy( (char*)&sent, (char*)&(buf[0]), sizeof(struct timeval) );
	gettimeofday(&now,NULL);

	tempoimpiegato=differenza(now,sent);
	msec=(tempoimpiegato.tv_sec*1000)+(tempoimpiegato.tv_usec/1000);

	return(msec);
}

/**
  * @brief Aggiorna l'indice della porta a cui stava inviando i pacchetti col nuovo
  * @param *cfg : puntatore al vettore di porte
  * @param *tmpporte : puntatore al nuovo vettore di porte
  * @return Restituisce il ritardo del pacchetto espresso in millisecondi.
 */
int config_new_porte(uint16_t *cfg, uint16_t *tmpporte)
{
	/* Aggiorna l'indice della porta a cui stava inviando i pacchetti con quello nuovo */
	i=0;
	while((cfg[index] != tmpporte[i]) && (i<numporte)) /* <= */
	{
		i++;
	}
	if(i < numporte) return(i);
	else return((index+1)%numporte);
}

#define PARAMETRIDEFAULT "./LBmobile.exe 6001 7001 8000"

/**
  * @brief Stampa a video i parametri utilizzati per avviare le applicazioni.
  * @return void.
 */
void usage(void) 
{  printf ( "usage:  ./LBmobile.exe LOCALPORTMOBILE LOCALPORTDATA PORTMONITOR\n"
			"esempio: "  PARAMETRIDEFAULT "\n");
}

/**
  * @brief Genera il pacchetto di configurazione delle porte da inviare al Fixed.
  * @param *cfg : puntatore al pacchetto di configurazione
  * @param numporte : numero di porte attive
  * @return void.
 */
void config_pkt_porte(uint32_t *cfg, uint32_t numporte)
{
	int i, j;
	
	/* Inizializza il pacchetto CFG */
	memset((char*)cfg,0,sizeof(cfg));
	
	/* Il suo ID è 0 */
	cfg[0] = 0;

	cfg[3] = numporte;

	/* Inserisce le porte nelle celle giuste */
	for(i=4 , j=0; i < SIZE_BUF_CFGPKT; i++, j++)
		cfg[i] = cfgPorte[j];
}

/**
  * @brief Gestisce la lettura dei pacchetti UDP provenienti dal Monitor.
  * @return void.
 */
void lettura_pkt_monitor(void)
{
	uint32_t buf[ (65536/sizeof(uint32_t)) ];
	struct sockaddr_in From;
	unsigned int Fromlen;
	int ris;

	memset(&From,0,sizeof(From));
	Fromlen=sizeof(struct sockaddr_in);
	ris = recvfrom ( monitordatafd, (char*)buf, (int)65536, 0, (struct sockaddr*)&From, &Fromlen);
	if (ris<0)
	{
		if(errno!=EINTR) 
		{	
			fprintf(stderr,"recvfrom() failed, Error: %d \"%s\"\n", errno,strerror(errno));
			fprintf(stderr,"ma non ho chiuso  il socket");
		}
	}
	else
	{
		int ris2;
		long int msecdelay;
		uint32_t idletto;
		uint16_t portfromfixed;

		/* Salva la porta da cui arrivano i pacchetti del Fixed */
		portfromfixed = ntohs(From.sin_port);
		
		/* Se legge un ping calcola il suo delay e lo imposta 0 come id */
		if(ris == SIZE_PING)
		{
			idletto=0;
			msecdelay=compute_delay_ping(buf);
		}

		/* Altrimenti lo gestisce come un normale pacchetto */
		else
		{
			idletto=buf[0];
			msecdelay=compute_delay(buf);
		}

		/* Se sono pacchetti normali e non ping, li inoltra all'applicazione */
		if(ris != SIZE_PING)
		{
			ris2=Sendn(appmobilefd,(char*)buf,ris);
			if(ris2!=ris) { fprintf(stderr,"recv from appmobile failed, received %d: ", ris); exit(9); }
		}

		/* Se arriva un pacchetto in ritardo e la porta da cui arriva è la stessa da cui invia, la cambia perché LENTA */
		if((msecdelay > PKT_IN_TIME) && (portfromfixed == cfgPorte[index]))
		{
			index=(index+1)%numporte;
		}
		/* Altrimenti se arriva un pacchetto in orario su una porta differente, si sincronizza su quella porta */
		else if((msecdelay <= PKT_IN_TIME) && (portfromfixed != cfgPorte[index]))
		{
			index=trova_porta(portfromfixed, cfgPorte, index, numporte);
		}
	}
}

/**
  * @brief Gestisce la lettura dei pacchetti UDP provenienti dall'Applicazione Mobile.
  * @return void.
 */
void lettura_pkt_app(void)
{
	uint32_t buf[PKTSIZE];
	uint32_t idmsg;

	/* ricevo un pkt */
	/*ris=Readn(appmobilefd,(char*)buf,PKTSIZE);*/
	ris=Readn(appmobilefd,(char*)buf,sizeof(buf));
	if(ris!=sizeof(buf)) { fprintf(stderr,"recv from appmobile failed, received %d: ", ris); exit(9); }

	idmsg = buf[0];

	/*ris=send_udp(monitordatafd, (char*)buf, PKTSIZE , 1000, "127.0.0.1", cfgPorte[index] );*/
	ris=send_udp(monitordatafd, (char*)buf, sizeof(buf) , 1000, "127.0.0.1", cfgPorte[index] );
	if(!(ris))
	{
		fprintf(stderr,"pkt id %u NOT sent\n", idmsg );
	}
	
	/* Inserisce il pacchetto nella lista */
	lPkt = ins_pkt(buf, lPkt, cfgPorte[index]);
}

/**
  * @brief Main
  * @return Ritorna un intero.
 */
int main(int argc, char *argv[])
{
	
	/* Inizializzazione delle variabili */
	index = 0;
	lPkt = NULL;
	contACK = 0;
	tipoCnt = 0;

	printf ("uso i parametri di default \n%s\n", PARAMETRIDEFAULT );
	localportfrommobile = 6001;
	localportdatamonitor = 7001;
	portmonitor = 8000;
	
	/* Inizializzazione dei vettori */
	initV(cfgPKT);
	initV(tipoConn);
	initV(pktTipoConn);
	
	init_random();

	/* Si connette al monitor */
	ris=TCP_setup_connection(&monitorfd, "127.0.0.1", portmonitor,  300000, 300000, 1);
	if(!ris) {	printf ("TCP_setup_connection() failed\n"); exit(1); }
	else printf("monitorfd %d\n", monitorfd);

	FD_ZERO(&all);
	FD_SET(monitorfd,&all);
	maxfd=monitorfd;

	ris=RecvCfg(monitorfd,&numporte,porte);
	if(!ris) {	printf ("RecvCfg() failed\n"); exit(1); }
	for(i=0;i<numporte;i++) {
		cfgPorte[i]=porte[i];
	}

	/* Creazione del pacchetto della configurazione delle porte */
	config_pkt_porte(cfgPKT, numporte);
	
	ris=UDP_setup_socket_bound( &monitordatafd, localportdatamonitor, 65535, 65535 );
	if (!ris) {	printf ("UDP_setup_socket_bound() failed\n"); exit(1); }
	FD_SET(monitordatafd,&all);
	if(monitordatafd>maxfd)
		maxfd=monitordatafd;

	appmobilefd=0;
	/* Attende la connessione dall'applicazione lato mobile */
	ris=TCP_setup_socket_listening( &listening_mobilefd, localportfrommobile, 300000, 300000, 1);
	if (!ris)
	{	printf ("TCP_setup_socket_listening() failed\n");
		exit(1);
	}
	do { memset (&Cli, 0, sizeof (Cli));		
		len = sizeof (Cli);
		appmobilefd = accept ( listening_mobilefd, (struct sockaddr *) &Cli, &len);
	} while ( (appmobilefd<0) && (errno==EINTR) );
	if (appmobilefd < 0 ) {	
		perror("accept() failed: \n");
		exit (1);
	}
	/* NO NAGLE per i tcp */
	ris=SetsockoptTCPNODELAY(appmobilefd,1); if (!ris) { fprintf(stderr,"unable to setup TCPNODELAY option\n"); exit(5); }
	/* chiusura del socket TCP listening e setup dei socket UDP	*/
	close(listening_mobilefd);
	FD_SET(appmobilefd,&all);
	if(appmobilefd>maxfd)
		maxfd=appmobilefd;
		
	/* Imprime il timestamp sul primo pacchetto, ossia fa partire il cronometro */
	gettimeofday(&timePktFirst,NULL);
	memcpy( (char*)&(tipoConn[0]), (char*)&timePktFirst, sizeof(struct timeval) );
	
	/* Controllo del tipo di connessione */
	for(;;)
	{
		do 
		{
			rdset=all;

#ifdef VICDEBUG
			stampa_fd_set("rdset prima",&rdset);
#endif

			ris=select(maxfd+1,&rdset,NULL,NULL,NULL);
		} while( (ris<0) && (errno==EINTR) );
		if(ris<0) 
		{
			perror("select failed: ");
			exit(1);
		}
			
		/* Controlla se sono arrivati pacchetti dall'applicazione */
		if( FD_ISSET(appmobilefd,&rdset) )
		{
			uint32_t buf[PKTSIZE];
			uint32_t idmsg;	
			struct timeval sent, now, tempoimpiegato;
			long int msec;

			/* riceve un pkt */
			/*ris=Readn(appmobilefd,(char*)buf,PKTSIZE);
			if(ris!=PKTSIZE) { fprintf(stderr,"recv from appmobile failed, received %d: ", ris); exit(9); }*/
			ris=Readn(appmobilefd,(char*)buf,sizeof(buf));
			if(ris!=sizeof(buf)) { fprintf(stderr,"recv from appmobile failed, received %d: ", ris); exit(9); }

			/* Appena arriva un pacchetto dall'AppMobile, calcola quanto tempo è passato */
			memcpy( (char*)&sent, (char*)&(tipoConn[0]), sizeof(struct timeval) );
			gettimeofday(&now,NULL);

			tempoimpiegato=differenza(now,sent);
			msec=(tempoimpiegato.tv_sec*1000)+(tempoimpiegato.tv_usec/1000);
			
			idmsg = buf[0];

			/* ID del pkt di connessione è 42, tanto per cambiare */
			pktTipoConn[0] = 42;
			
			/* Può capitare che arrivi il primo pacchetto con 40msec di delay su connessione LENTA */
			/* Se è il primo pacchetto */
			if(idmsg < 1)
			{
				/* E se il primo pacchetto arriva con abbastanza ritardo, vuol dire che la connessione è LENTA */
				if((msec > DELAY_VELOCE) && (msec < DELAY_MUTA))
				{
					tipoCnt = pktTipoConn[1] = CONN_LENTA;
					break;
				}
				/* Altrimenti sarà MUTA */
				else if(msec >= DELAY_MUTA)
				{
					tipoCnt = pktTipoConn[1] = CONN_MUTA;
					break;
				}
			}
			/* Se invece il secondo pacchetto è veloce, vuol dire che siamo nella VELOCE */
			else if(msec <= DELAY_LENTA)
			{
				tipoCnt = pktTipoConn[1] = CONN_VELOCE;
				break;
			}
			/* Altrimenti è LENTA */
			else
			{
				tipoCnt = pktTipoConn[1] = CONN_LENTA;
				break;
			}

		}
	}
	
		/* Invio il tipo di connessione al Fixed su tutte le porte attive per avvertirlo */
		/* Invio il tipo di connessione su ogni porta attiva per ben due volte (per evitare di gestire i suoi NACK) */
		for(k=0; k<2; k++)
		{
			for(i=0; i<numporte; i++)
			{
				ris = send_udp(monitordatafd, (char*)pktTipoConn, sizeof(pktTipoConn) , 1000, "127.0.0.1", cfgPorte[i]);
				if(!(ris))
				{
					fprintf(stderr,"Errore durante l'invio del tipo di connessione!\n");
				}
			}
		}

	send_config(cfgPKT, monitordatafd, cfgPorte, numporte);
	
	/* Configurazione del LBMobile sul tipo di connessione */
	switch(tipoCnt)
	{
		case CONN_MUTA:
		
#ifdef TIPO_CONN
			fprintf(stdout,ROSSO "\n*** Attivazione della connessione MUTA ***\n" DEFAULTCOLOR "\n");
#endif
			
/* *********************************************** MUTA ************************************************** */

	for(;;)
	{
		do {
			rdset=all;

#ifdef VICDEBUG
			stampa_fd_set("rdset prima",&rdset);
#endif

			ris=select(maxfd+1,&rdset,NULL,NULL,NULL);
		} while( (ris<0) && (errno==EINTR) );
		if(ris<0) {
			perror("select failed: ");
			exit(1);
		}

#ifdef VICDEBUG
		stampa_fd_set("rdset dopo",&rdset);
#endif

		/* Lettura dei pacchetti UDP provenienti dal Monitor */
		if(FD_ISSET(monitordatafd,&rdset))
		{
			lettura_pkt_monitor();		
		}

		/* Lettura dei pacchetti TCP (CONF, ACK, NACK) provenienti dal Monitor */
		if( FD_ISSET(monitorfd,&rdset) )
		{
			char ch; int ris;
			uint32_t tmpnumporte;
			uint16_t tmpporte[MAXNUMCONNECTIONS];
			uint32_t idmsg;

			/* ris=recv(monitorfd,buf,65536,MSG_DONTWAIT); */
			ris=recv(monitorfd,&ch,1,0);
			if(ris!=1) { fprintf(stderr,"recv from monitor failed: "); exit(9); }
			
			if(ch=='C')
			{
				/* Ricezione del nuovo numero di porte attive */
				ris=recv(monitorfd,(char*)&tmpnumporte,sizeof(uint32_t),0);
				if(ris!=sizeof(uint32_t)) { fprintf(stderr,"(C)recv_configurazione failed: "); exit(9); }

				/* Ricezione del nuovo vettore di porte attive */
				for(i=0;i<tmpnumporte;i++)
				{
 					ris=recv(monitorfd,&(tmpporte[i]),sizeof(uint16_t),0);
					if(ris!=sizeof(uint16_t)) { fprintf(stderr,"(C%d)recv_configurazione failed: ",i); exit(9); }
				}
				
				/* Aggiorna l'indice della porta a cui stava inviando i pacchetti col nuovo */
				config_new_porte(cfgPorte, tmpporte);

				setup_new_configurazione(&numporte,porte,&tmpnumporte,tmpporte);

				/* Creazione del pacchetto della configurazione delle porte */
				config_pkt_porte(cfgPKT, numporte);
				
				/* Invio della configurazione al Fixed */
				send_config(cfgPKT, monitordatafd, cfgPorte, numporte);

			} 
			else if( ch=='A' )
			{
				ris=recv(monitorfd,(char*)&idmsg,sizeof(uint32_t),0);
				if(ris!=sizeof(uint32_t)) { fprintf(stderr,"(A)recv ack failed: "); exit(9); }

				/* Se arriva un ACK presente nella lista di pacchetti, lo rimuove */
				if(find_id_pkt(idmsg, lPkt) != NULL)
				{
					lPkt = rim_pkt(idmsg, lPkt);
				}
			} 
			else if( ch=='N' )
			{
				struct listaPKT *list_aux;

				list_aux = NULL;

				ris=recv(monitorfd,(char*)&idmsg,sizeof(uint32_t),0);
				if(ris!=sizeof(uint32_t)) { fprintf(stderr,"(N)recv nack failed: "); exit(9); }

				/* Restituisce il puntatore lista all'elemento interessato */
				list_aux = find_id_pkt(idmsg, lPkt);

				/* Se il pacchetto NACKato è presente nella lista */
				if(list_aux != NULL)
				{
					/* E se la porta da cui è stato inviato il pacchetto è uguale all'attuale da cui s'invia */
					if(list_aux->portaPKT == cfgPorte[index])
					{
						/* Viene cambiata */
						index=(index+1)%numporte;
					}
					/* Aggiorna anche la porta d'invio del pacchetto */
					list_aux->portaPKT = cfgPorte[index];
					
					/* Rispedisce l'esatto pacchetto che è stato NACKato */
					ris=send_udp(monitordatafd, (char*)list_aux->pkt, sizeof(list_aux->pkt) , 1000, "127.0.0.1", cfgPorte[index] );
					if(!(ris))
					{
						fprintf(stderr,"pkt id %u NOT sent\n", list_aux->pkt[0] );
					}
				}
			}

		}
		
		/* Controlla se sono arrivati pacchetti dall'applicazione */
		if( FD_ISSET(appmobilefd,&rdset) )
		{
			lettura_pkt_app();
		}
		
	} /* fine for ;; */
	
/***********************************************************************************************************/

			break;
		case CONN_VELOCE:
				
#ifdef TIPO_CONN
			fprintf(stderr,GREEN "\n*** Attivazione della connessione VELOCE ***\n" DEFAULTCOLOR "\n");
#endif
			
/* *********************************************** VELOCE ************************************************** */

	for(;;)
	{
		do {
			rdset=all;

#ifdef VICDEBUG
			stampa_fd_set("rdset prima",&rdset);
#endif

			ris=select(maxfd+1,&rdset,NULL,NULL,NULL);
		} while( (ris<0) && (errno==EINTR) );
		if(ris<0) {
			perror("select failed: ");
			exit(1);
		}

#ifdef VICDEBUG
		stampa_fd_set("rdset dopo",&rdset);
#endif

		/* Lettura dei pacchetti UDP provenienti dal Monitor */
		if(FD_ISSET(monitordatafd,&rdset))
		{
			lettura_pkt_monitor();
		}

		/* Lettura dei pacchetti TCP (CONF, ACK, NACK) provenienti dal Monitor */
		if( FD_ISSET(monitorfd,&rdset) )
		{
			char ch; int ris;
			uint32_t tmpnumporte;
			uint16_t tmpporte[MAXNUMCONNECTIONS];
			uint32_t idmsg;

			/* ris=recv(monitorfd,buf,65536,MSG_DONTWAIT); */
			ris=recv(monitorfd,&ch,1,0);
			if(ris!=1) { fprintf(stderr,"recv from monitor failed: "); exit(9); }

			if(ch=='C') 
			{
				ris=recv(monitorfd,(char*)&tmpnumporte,sizeof(uint32_t),0);
				if(ris!=sizeof(uint32_t)) { fprintf(stderr,"(C)recv_configurazione failed: "); exit(9); }

				for(i=0;i<tmpnumporte;i++)
				{
 					ris=recv(monitorfd,&(tmpporte[i]),sizeof(uint16_t),0);
					if(ris!=sizeof(uint16_t)) { fprintf(stderr,"(C%d)recv_configurazione failed: ",i); exit(9); }

				}
				
				config_new_porte(cfgPorte, tmpporte);

				setup_new_configurazione(&numporte,porte,&tmpnumporte,tmpporte);

				/* Creazione del pacchetto della configurazione delle porte */
				config_pkt_porte(cfgPKT, numporte);
				
				send_config(cfgPKT, monitordatafd, cfgPorte, numporte);
			} 
			else if( ch=='A' ) 
			{
				ris=recv(monitorfd,(char*)&idmsg,sizeof(uint32_t),0);
				if(ris!=sizeof(uint32_t)) { fprintf(stderr,"(A)recv ack failed: "); exit(9); }

				/* Se arriva un ACK presente nella lista di pacchetti, lo rimuove */
				if(find_id_pkt(idmsg, lPkt) != NULL)
				{
					lPkt = rim_pkt(idmsg, lPkt);
				}
				
				/* Incrementa il contatore degli ACK */
				contACK++;
			} 
			else if( ch=='N' ) 
			{
				struct listaPKT *list_aux;

				list_aux = NULL;

				ris=recv(monitorfd,(char*)&idmsg,sizeof(uint32_t),0);
				if(ris!=sizeof(uint32_t)) { fprintf(stderr,"(N)recv nack failed: "); exit(9); }

				/* Restituisce il puntatore lista all'elemento interessato */
				list_aux = find_id_pkt(idmsg, lPkt);

				/* Se il pacchetto NACKato è presente nella lista */
				if(list_aux != NULL)
				{
					/* Se precedentemente sono arrivati almeno due ACK dietro fila, vuol dire che attualmente il Mobile
					   è connesso sulla porta 'OK', quindi viene data una seconda chance al pacchetto rinviandolo sulla
					   medesima porta, altrimenti si scorre il vettore di porte dato che due NACK sulla stessa porta
					   indicano il cambiamento in 'LOSS' */

					if(contACK < NUM_CONTACK)
					{
						if(list_aux->portaPKT == cfgPorte[index])
						{
							index=(index+1)%numporte;
						}
					}
					else
					{
						contACK = 0;
					}

					/* Aggiorna anche la porta d'invio del pacchetto */
					list_aux->portaPKT = cfgPorte[index];
					
					/* Rispedisce l'esatto pacchetto che è stato NACKato */
					ris=send_udp(monitordatafd, (char*)list_aux->pkt, sizeof(list_aux->pkt) , 1000, "127.0.0.1", cfgPorte[index] );
					if(!(ris))
					{
						fprintf(stderr,"pkt id %u NOT sent\n", list_aux->pkt[0] );
					}
				}
			}

		}
		
		/* Controlla se sono arrivati pacchetti dall'applicazione */
		if( FD_ISSET(appmobilefd,&rdset) )
		{
			lettura_pkt_app();
		}


	} /* fine for ;; */
	
/***********************************************************************************************************/
			
			break;
		case CONN_LENTA:
						
#ifdef TIPO_CONN
			fprintf(stdout,ORANGE "\n*** Attivazione della connessione LENTA ***\n" DEFAULTCOLOR "\n");
#endif
			
/* *********************************************** LENTA ************************************************** */

	for(;;)
	{
		do {
			rdset=all;

#ifdef VICDEBUG
			stampa_fd_set("rdset prima",&rdset);
#endif

			ris=select(maxfd+1,&rdset,NULL,NULL,NULL);
		} while( (ris<0) && (errno==EINTR) );
		if(ris<0) {
			perror("select failed: ");
			exit(1);
		}

#ifdef VICDEBUG
		stampa_fd_set("rdset dopo",&rdset);
#endif

		/* Lettura dei pacchetti UDP provenienti dal Monitor */
		if(FD_ISSET(monitordatafd,&rdset))
		{
			lettura_pkt_monitor();
		}

		/* Lettura dei pacchetti TCP (CONF, ACK, NACK) provenienti dal Monitor */
		if( FD_ISSET(monitorfd,&rdset) )
		{
			char ch; int ris;
			uint32_t tmpnumporte;
			uint16_t tmpporte[MAXNUMCONNECTIONS];
			uint32_t idmsg;

			/* ris=recv(monitorfd,buf,65536,MSG_DONTWAIT); */
			ris=recv(monitorfd,&ch,1,0);
			if(ris!=1) { fprintf(stderr,"recv from monitor failed: "); exit(9); }

			if(ch=='C') 
			{	
				ris=recv(monitorfd,(char*)&tmpnumporte,sizeof(uint32_t),0);
				if(ris!=sizeof(uint32_t)) { fprintf(stderr,"(C)recv_configurazione failed: "); exit(9); }

				for(i=0;i<tmpnumporte;i++)
				{
 					ris=recv(monitorfd,&(tmpporte[i]),sizeof(uint16_t),0);
					if(ris!=sizeof(uint16_t)) { fprintf(stderr,"(C%d)recv_configurazione failed: ",i); exit(9); }

				}
				
				config_new_porte(cfgPorte, tmpporte);

				setup_new_configurazione(&numporte,porte,&tmpnumporte,tmpporte);

				/* Creazione del pacchetto della configurazione delle porte */
				config_pkt_porte(cfgPKT, numporte);
				
				send_config(cfgPKT, monitordatafd, cfgPorte, numporte);
			} 
			else if( ch=='A' ) 
			{
				ris=recv(monitorfd,(char*)&idmsg,sizeof(uint32_t),0);
				if(ris!=sizeof(uint32_t)) { fprintf(stderr,"(A)recv ack failed: "); exit(9); }

				/* Se arriva un ACK presente nella lista di pacchetti, lo rimuove */
				if(find_id_pkt(idmsg, lPkt) != NULL)
				{
					lPkt = rim_pkt(idmsg, lPkt);
				}
			} 
			else if( ch=='N' ) 
			{
				struct listaPKT *list_aux;

				list_aux = NULL;

				ris=recv(monitorfd,(char*)&idmsg,sizeof(uint32_t),0);
				if(ris!=sizeof(uint32_t)) { fprintf(stderr,"(N)recv nack failed: "); exit(9); }

				/* Restituisce il puntatore lista all'elemento interessato */
				list_aux = find_id_pkt(idmsg, lPkt);

				/* Se il pacchetto NACKato è presente nella lista */
				if(list_aux != NULL)
				{
					/* Se non sono arrivati dei NACK precedentemente, viene data la seconda chance all'attuale pacchetto
					   NACKato, altrimenti si scorre il vettore di porte */
					if(contACK > 0)
					{
						if(list_aux->portaPKT == cfgPorte[index])
						{
							index=(index+1)%numporte;
						}
						contACK = 0;
					}
					/* Aggiorna anche la porta d'invio del pacchetto */
					list_aux->portaPKT = cfgPorte[index];
					
					/* Rispedisce l'esatto pacchetto che è stato NACKato */
					ris=send_udp(monitordatafd, (char*)list_aux->pkt, sizeof(list_aux->pkt) , 1000, "127.0.0.1", cfgPorte[index] );
					if(!(ris))
					{
						fprintf(stderr,"pkt id %u NOT sent\n", list_aux->pkt[0] );
					}
					
					contACK++;
				}
			}

		}
		
		/* Controlla se sono arrivati pacchetti dall'applicazione */
		if( FD_ISSET(appmobilefd,&rdset) )
		{
			lettura_pkt_app();
		}


	} /* fine for ;; */
	
/***********************************************************************************************************/
			
			break;
		default:
			fprintf(stderr,"Errore durante la configurazione del tipo di connessione.\n");
			exit(1);
			break;
	}
	
	close(monitorfd);
	return(0);
}
