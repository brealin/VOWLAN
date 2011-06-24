/**
 *  @file const.h
 *  @author Vincenzo Ferrari - Barbara Iadarola - Luca Giuliano
 *  @brief Header delle costanti.
 *  @note Questo file header contiene la dichiarazione delle costanti utilizzate dai due Load Balancer.
 */
 
/* LBMobile CONST */

#define SIZE_BUF_CFGPKT 8	/* dimensione del pacchetto contenente la configurazione delle porte */
#define SIZE_TIPOCONN 2		/* dimensione del pacchetto del tipo di connessione */
#define SIZE_PING 8		/* dimensione del pacchetto PING */

#define DELAY_VELOCE 60		/* tempo minimo espresso in millisecondi per l'arrivo del primo pacchetto veloce */
#define DELAY_LENTA 100		/* tempo minimo espresso in millisecondi per l'arrivo del secondo pacchetto sommato al primo veloce */
#define DELAY_MUTA 10000	/* tempo minimo espresso in millisecondi per l'arrivo del primo pacchetto muto */

#define NUM_CONTACK 2		/* contatore degli ACK arrivati dietro fila per la seconde chance */

/* LBFixed CONST */

#define SIZE_PKT_TIPOCONN 8	/* dimensione del pacchetto del tipo di connessione (IN BYTE) */
#define SIZE_CFGPKT 32		/* dimensione del pacchetto di configurazione delle porte (IN BYTE) */
#define DELAY_PING 500		/* tempo minimo espresso in millisecondi per l'invio dei PING dal Fixed al Mobile sulla connessione MUTA */
#define SIZE_BUF_TIMEVAL 2	/* dimensione del pacchetto contenente la struttura timeval */

/* Entrambi CONST */

#define CONN_MUTA 0		/* parametro che specifica la connessione MUTA */
#define CONN_VELOCE 1		/* parametro che specifica la connessione VELOCE */
#define CONN_LENTA 2		/* parametro che specifica la connessione LENTA */

#define PKT_IN_TIME 150		/* millisecondi massimi validi per un pacchetto in orario */
