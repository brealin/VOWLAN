/**
 *  @file LBfixed.c
 *  @author Vincenzo Ferrari - Barbara Iadarola - Luca Giuliano
 *  @brief Gestione della connessione Wired
 *  @note Questo file consente la gestione della connessione Wired tra l'Applicazione Fixed e il Monitor
 */

#include "../include/LB.h"

/* #define VICDEBUG */
#define TIPO_CONN

#define P(X) do { fprintf(stderr,X "\n"); fflush(stderr); } while(0)

fd_set rdset, all;
int maxfd;
int numspediti=0;
int numscartati=0;
int printed=0;

#define PARAMETRIDEFAULT "./LBfixed.exe 11001 10001"

/**
  * @brief Stampa a video i parametri utilizzati per avviare le applicazioni.
  * @return void.
 */
void usage(void) 
{  printf ( "usage:  ./LBfixed.exe LOCALPORTFIXED LOCALPORTDATA\n"
			"esempio: "  PARAMETRIDEFAULT "\n");
}

/**
  * @brief Main
  * @return Ritorna un intero.
 */
int main(int argc, char *argv[])
{
	uint16_t localportfromfixed, localportdatamonitor;
	struct sockaddr_in Cli;
	unsigned int len;
	int32_t monitordatafd;
	int ris, listening_fixedfd, appfixedfd;
	int i, j;
	
	/* VARIABILI PER LA CONNESSIONE LENTA */
	struct timeval tempoimpiegato, now, sent;
	long int mseccron;

	/* Timestamp per l'invio dei PING */
	struct timeval cronometro;

	/* Variabile per il calcolo del tempo per l'invio dei ping */
	uint32_t cronPKT[SIZE_BUF_TIMEVAL];
	
	/* Contatore dei pacchetti lenti: se arrivano tot pacchetti su una porta lenta, è ora di aggiornarcisi sopra! */
	int contLenti;
	
	/* Per evitare di gestire i pacchetti appena arrivati con id più giovane */
	/* Es:	pkt 10 msec 150
		pkt 8  msec 230
		pkt 9  msec 210 */
	uint32_t idOld;

	/* Flag per il randomport */
	/* 0 : disattivata */
	/* 1 : attivata */
	int pktOnDelay;

	/* Vettore di porte */
	uint16_t cfgPorte[MAXNUMCONNECTIONS];

	/* Indice per il vettore di porte */
	int index;

	/* Porte attive del Monitor */
	uint32_t numporte;
	
	/* Tipo di connessione avviata (0 = muta, 1 = veloce, 2 = lenta) */
	uint32_t tipoCnt;

	/* Inizializzazione delle variabili */
	numporte = 0;
	pktOnDelay = 0;
	index = 0;
	idOld = 0;
	contLenti = 0;
	tipoCnt = 0;
	
	/* Inizializzazione del vettore di porte */
	for(i=0, j=1;i<MAXNUMCONNECTIONS;i++, j++)
		cfgPorte[i] = 9000 + j;

	printf ("uso i parametri di default \n%s\n", PARAMETRIDEFAULT );
	localportfromfixed = 11001;
	localportdatamonitor = 10001;

	init_random();

	ris=UDP_setup_socket_bound( &monitordatafd, localportdatamonitor, 65535, 65535 );
	if (!ris) {	printf ("UDP_setup_socket_bound() failed\n"); exit(1); }

	FD_ZERO(&all);
	FD_SET(monitordatafd,&all);
	if(monitordatafd>maxfd)
		maxfd=monitordatafd;

	appfixedfd=0;
	/* Attende connessione dalla applicazione lato fixed */
	ris=TCP_setup_socket_listening( &listening_fixedfd, localportfromfixed, 300000, 300000, 1);
	if (!ris)
	{	printf ("TCP_setup_socket_listening() failed\n");
		exit(1);
	}
	do { memset (&Cli, 0, sizeof (Cli));		
		len = sizeof (Cli);
		appfixedfd = accept ( listening_fixedfd, (struct sockaddr *) &Cli, &len);
	} while ( (appfixedfd<0) && (errno==EINTR) );
	if (appfixedfd < 0 ) {	
		perror("accept() failed: \n");
		exit (1);
	}
	/* NO NAGLE per i tcp */
	ris=SetsockoptTCPNODELAY(appfixedfd,1); if (!ris) { fprintf(stderr,"unable to setup TCPNODELAY option\n"); exit(5); }

	/* chiusura del socket TCP listening e setup dei socket UDP	*/
	close(listening_fixedfd);
	FD_SET(appfixedfd,&all);
	if(appfixedfd>maxfd)
		maxfd=appfixedfd;
	
	/* Controllo del tipo di connessione avviata */
	while(1)
	{
		do {
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

#ifdef VICDEBUG
		stampa_fd_set("rdset dopo",&rdset);
#endif

		/* Controlla se sono arrivati pacchetti UDP dal Monitor */
		if(FD_ISSET(monitordatafd,&rdset))  
		{
			uint32_t buf[ (65536/sizeof(uint32_t)) ];
			struct sockaddr_in From;
			unsigned int Fromlen;
			int risConn;

			memset(&From,0,sizeof(From));
			Fromlen=sizeof(struct sockaddr_in);
			risConn = recvfrom ( monitordatafd, (char*)buf, (int)65536, 0, (struct sockaddr*)&From, &Fromlen);
			if (risConn<0) 
			{
				if(errno!=EINTR) 
				{	
					fprintf(stderr,"recvfrom() failed, Error: %d \"%s\"\n", errno,strerror(errno));
					fprintf(stderr,"ma non ho chiuso  il socket");
				}
			}
			/* Se il pacchetto letto è quello del tipo di connessione, lo salva nella variabile tipoCnt */
			else if(risConn <= SIZE_PKT_TIPOCONN)
			{
				tipoCnt = buf[1];
				break;
				
			}
		}
	}

	/* Configurazione del LBMobile sul tipo di connessione */
	switch(tipoCnt)
	{
		case CONN_MUTA:
		
#ifdef TIPO_CONN
			fprintf(stderr,ROSSO "\n*** Attivazione della connessione MUTA ***\n" DEFAULTCOLOR "\n");
#endif

			/* Inizializza il cronometro */
			gettimeofday(&cronometro,NULL);
			memcpy( (char*)&(cronPKT[0]), (char*)&cronometro, sizeof(struct timeval) );

/* *********************************************** MUTA ************************************************** */

			for(;;)
			{
				fflush(stdout);
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

				/* Controlla se ci sono nuovi pacchetti dall'AppFixed */
				if( FD_ISSET(appfixedfd,&rdset) )
				{
					uint32_t buf[PKTSIZE];
					uint32_t idmsg;

					/* Riceve un pkt */
					ris=Readn(appfixedfd,(char*)buf,sizeof(buf));
					if(ris!=sizeof(buf)) { fprintf(stderr,"recv from appfixed failed, received %d: ", ris); exit(9); }

					/* Spedisce il pkt */
					idmsg=buf[0];
					ris=send_udp(monitordatafd, (char*)buf, sizeof(buf) , 1000, "127.0.0.1", cfgPorte[index]);
					if(!(ris))
					{
						fprintf(stderr,"pkt id %u NOT sent\n", idmsg );
					}
				}


				/* Controlla se sono arrivati pacchetti UDP dal Monitor */
				if(FD_ISSET(monitordatafd,&rdset))  
				{
					uint32_t buf[ (65536/sizeof(uint32_t)) ];
					struct sockaddr_in From;
					unsigned int Fromlen;
					int ris;

					memset(&From,0,sizeof(From));
					Fromlen=sizeof(struct sockaddr_in);
					ris = recvfrom(monitordatafd, (char*)buf, (int)65536, 0, (struct sockaddr*)&From, &Fromlen);
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
						/* Porta del pacchetto proveniente dal Monitor */
						uint16_t portFromMonitor;
						uint16_t portaAttuale;
						int i, j;
						
						/* Evita di gestire ulteriori pacchetti del tipo di connessioni rimasti nei socket */
						if(ris != SIZE_PKT_TIPOCONN)
						{
						
							idletto=buf[0];
							msecdelay=compute_delay(buf);

							/* Se il pacchetto è quello della configurazione delle porte */
							if(ris == SIZE_CFGPKT)
							{
								numporte=buf[3];
								/* Si salva da una parte la porta attuale per ripristinarla in seguito */
								portaAttuale = cfgPorte[index];

								/* Aggiorna il vettore di porte */
								for(i=0, j=4; i<numporte; i++, j++)
									cfgPorte[i]=(buf[j]+1000);
								
								config_porte(buf, numporte, cfgPorte, index, portaAttuale);
							}
							
							portFromMonitor = ntohs(From.sin_port);
							
							/* Inoltra all'applicazione solo i pacchetti voce */
							if(ris != SIZE_CFGPKT)
							{
								/* Spedisce all'applicazione */
								ris2=Sendn(appfixedfd,(char*)buf,ris);
								if(ris2!=ris) { fprintf(stderr,"recv from appfixedfd failed, received %d: ", ris); exit(9); }
							}

							/* Se il pacchetto è in orario e la porta d'invio è diversa da quella d'arrivo si sincronizza */
							if((msecdelay<=PKT_IN_TIME) && (portFromMonitor != cfgPorte[index]))
								index=trova_porta(portFromMonitor, cfgPorte, index, numporte);
						}
					}
				}
				
				/* Controlla il tempo d'invio dei PING */
				memcpy( (char*)&sent, (char*)&(cronPKT[0]), sizeof(struct timeval) );
				gettimeofday(&now,NULL);

				tempoimpiegato=differenza(now,sent);
				mseccron=(tempoimpiegato.tv_sec*1000)+(tempoimpiegato.tv_usec/1000);
				
				/* Se il tempo è trascorso, invia i PING su tutte le porte attive */		
				if(mseccron >= DELAY_PING)
				{
					/* Invio dei ping */
					send_ping(numporte, cfgPorte, monitordatafd);

					/* Azzeramento del cronometro */
					gettimeofday(&cronometro,NULL);
					memcpy( (char*)&(cronPKT[0]), (char*)&cronometro, sizeof(struct timeval) );
				}

			}

/* ******************************************************************************************************* */
			
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
				/* Controlla se ci sono nuovi pacchetti dall'AppFixed */
				if( FD_ISSET(appfixedfd,&rdset) )
				{
					uint32_t buf[PKTSIZE];
					uint32_t idmsg;

					/* Riceve un pkt */
					/*ris=Readn(appfixedfd,(char*)buf,PKTSIZE);
					if(ris!=PKTSIZE) { fprintf(stderr,"recv from appfixed failed, received %d: ", ris); exit(9); }*/
					ris=Readn(appfixedfd,(char*)buf,sizeof(buf));
					if(ris!=sizeof(buf)) { fprintf(stderr,"recv from appfixed failed, received %d: ", ris); exit(9); }

					/* Spedisco il pkt */
					idmsg=buf[0];
					/*ris=send_udp(monitordatafd, (char*)buf, PKTSIZE , 1000, "127.0.0.1", cfgPorte[index] );*/
					ris=send_udp(monitordatafd, (char*)buf, sizeof(buf) , 1000, "127.0.0.1", cfgPorte[index] );
					if(!(ris))
					{
						fprintf(stderr,"pkt id %u NOT sent\n", idmsg );
					}
					
					/* Se riceve pacchetti dalla porta lenta, comincia a inviare i pacchetti a porte diverse */
					if(pktOnDelay) index=(index+1)%numporte;
				}

				/* Controlla se sono arrivati pacchetti UDP dal Monitor */
				if(FD_ISSET(monitordatafd,&rdset))
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
						/* Porta del pacchetto proveniente dal Monitor */
						uint16_t portFromMonitor;
						uint16_t portaAttuale;
						int i,j;
						
						/* Evita di gestire ulteriori pacchetti del tipo di connessioni rimasti nei socket */
						if(ris != SIZE_PKT_TIPOCONN)
						{
							idletto=buf[0];
							msecdelay=compute_delay(buf);

							/* Se il pacchetto è quello della configurazione delle porte */
							if(ris == SIZE_CFGPKT)
							{
								numporte=buf[3];
								/* Si salva da una parte la porta attuale per ripristinarla in seguito */
								portaAttuale = cfgPorte[index];

								/* Aggiorna il vettore di porte */
								for(i=0, j=4; i<numporte; i++, j++)
									cfgPorte[i]=(buf[j]+1000);
								
								config_porte(buf, numporte, cfgPorte, index, portaAttuale);
							}

							/* Salva la porta del pacchetto appena giunto dal Mobile */
							portFromMonitor = ntohs(From.sin_port);

							/* Vengono inoltrati solo i pacchetti voce all'applicazione */
							if(ris != SIZE_CFGPKT)
							{
								/* Spedisce all'applicazione */
								ris2=Sendn(appfixedfd,(char*)buf,ris);
								if(ris2!=ris) { fprintf(stderr,"recv from appfixedfd failed, received %d: ", ris); exit(9); }
							}
						
							/* I pacchetti precedenti non li conta (es: pkt 10, poi pkt 8, poi pkt 11...) */
							if(idletto > idOld)
							{
								/* Invio dei ping */
								send_ping(numporte, cfgPorte, monitordatafd);
												
								/* Se il pacchetto arriva in ritardo e non è la configurazione 
								   (perché probabilmente è arrivata anche quella in OK) */
								if(msecdelay > PKT_IN_TIME)
								{
									if(ris != SIZE_CFGPKT)
									{
										/* Se sono arrivati altri pacchetti in ritardo e la flag
										   è disattivata, allora la attiva e azzera il contatore */
										if((contLenti > 0) && (!(pktOnDelay)))
										{
											pktOnDelay = 1;
											contLenti = 0;
										}
										contLenti++;
									}

								}
								/* Se il pacchetto è in orario e la porta d'invio è diversa da quella d'arrivo 
								   ci si sincronizza */
								else if((msecdelay <= PKT_IN_TIME))
								{
									if(portFromMonitor != cfgPorte[index])
										index=trova_porta(portFromMonitor, cfgPorte, index, numporte);
								
									/* Per ogni pacchetto in orario, si decrementa il contatore */
									if(contLenti > 0) contLenti--;
									pktOnDelay = 0;
								}
								
								/* Salvataggio dell'id del pacchetto attuale */
								idOld = idletto;
							}
						}
					}
				}
			}

/* ******************************************************************************************************* */
			
			break;
		case CONN_LENTA:
				
#ifdef TIPO_CONN
			fprintf(stderr,ORANGE "\n*** Attivazione della connessione LENTA ***\n" DEFAULTCOLOR "\n");
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

				/* Controlla se ci sono nuovi pacchetti dall'AppFixed */
				if( FD_ISSET(appfixedfd,&rdset) )
				{
					uint32_t buf[PKTSIZE];
					uint32_t idmsg;

					/* Riceve un pkt */
					ris=Readn(appfixedfd,(char*)buf,sizeof(buf));
					if(ris!=sizeof(buf)) { fprintf(stderr,"recv from appfixed failed, received %d: ", ris); exit(9); }

					/* Spedisce il pkt */
					idmsg=buf[0];
					ris=send_udp(monitordatafd, (char*)buf, sizeof(buf) , 1000, "127.0.0.1", cfgPorte[index] );
					if(!(ris))
					{
						fprintf(stderr,"pkt id %u NOT sent\n", idmsg );
					}

					/* Se il randomport è attivato, i PING vengono inviati su ogni porta attiva */
					if(pktOnDelay)
					{
						/* Invio dei ping */
						send_ping(numporte, cfgPorte, monitordatafd);
					}
					
					/* Se riceve dei pacchetti dalla porta lenta, comincia a inviare ogni pacchetto a porte diverse */
					if(pktOnDelay) index=(index+1)%numporte;
				}

				/* Controlla se sono arrivati pacchetti UDP dal Monitor */
				if(FD_ISSET(monitordatafd,&rdset))
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
						/* Porta del pacchetto proveniente dal Monitor */
						uint16_t portFromMonitor;
						uint16_t portaAttuale;
						int i,j;
						
						/* Evita di gestire ulteriori pacchetti del tipo di connessioni rimasti nei socket */
						if(ris != SIZE_PKT_TIPOCONN)
						{						
							idletto=buf[0];
							msecdelay=compute_delay(buf);

							/* Se il pacchetto è quello della configurazione delle porte */
							if(ris == SIZE_CFGPKT)
							{
								numporte=buf[3];
								/* Si salva da una parte la porta attuale per ripristinarla in seguito */
								portaAttuale = cfgPorte[index];

								/* Aggiorna il vettore di porte */
								for(i=0, j=4; i<numporte; i++, j++)
									cfgPorte[i]=(buf[j]+1000);
								
								config_porte(buf, numporte, cfgPorte, index, portaAttuale);
							}
							portFromMonitor = ntohs(From.sin_port);

							/* Vengono inoltrate all'applicazioni solo i pacchetti voce */
							if(ris != SIZE_CFGPKT)
							{
								/* Spedisce all'applicazione */
								ris2=Sendn(appfixedfd,(char*)buf,ris);
								if(ris2!=ris) { fprintf(stderr,"recv from appfixedfd failed, received %d: ", ris); exit(9); }
							}

							/* Se arrivano dei pacchetti in ritardo */
							if(msecdelay > PKT_IN_TIME)
							{
								/* E se non sono pacchetti di configurazione */
								if(ris != SIZE_CFGPKT)
								{
									/* Se il randomport non è stato ancora attivato, lo attiva */
									if(!(pktOnDelay)) pktOnDelay = 1;
									/* Se è arrivato un pacchetto in ritardo sulla porta da cui sta inviando 
									   e il randomport non è attivo cambia la porta */
									if(pktOnDelay)
										index=(index+1)%numporte;
								}

								/* Invio dei ping */
								send_ping(numporte, cfgPorte, monitordatafd);
															}
							/* Se il pacchetto è in orario e la porta d'invio è diversa da quella d'arrivo
							   si sincronizza */
							else if((msecdelay<=PKT_IN_TIME))
							{
								if(portFromMonitor != cfgPorte[index])
									index=trova_porta(portFromMonitor, cfgPorte, index, numporte);
								
								/* Azzera la flag se attiva */
								if(pktOnDelay) pktOnDelay = 0;
								
							}
						}
					}
				}
			}
			
/* ******************************************************************************************************* */
			
			break;
		default:
			fprintf(stderr,"Errore durante la configurazione del tipo di connessione.\n");
			exit(1);
			break;
	}

	return(0);
}
