/**
 *  @file LBliste.c
 *  @author Vincenzo Ferrari - Barbara Iadarola - Luca Giuliano
 *  @brief Collezione di funzioni su liste
 *  @note Questo file contiene le funzioni usate dal Load Balancer Mobile per gestire le liste di pacchetti.
 */

#define __LBLISTE_C__

#include "../include/LBliste.h"


/**
  * @brief Inserisce un elemento nella coda della lista.
  * @param *insPKT : puntatore al pacchetto da inserire
  * @param *testa : puntatore alla testa della lista
  * @param portapacchetto : porta dalla quale il pacchetto è stato inviato
  * @return Restituisce una nuova lista contenente un nuovo elemento.
 */
struct listaPKT *ins_pkt(uint32_t *insPKT, struct listaPKT *testa, uint16_t portapacchetto)
{

	struct listaPKT *pktIndex, *newPKT;
	int i;

	/* Salvataggio della testa della lista */
	pktIndex = testa;

	if(pktIndex != NULL) /* se la lista non è vuota */
	{
		/* scorre la lista fino ad arrivare al penultimo elemento (quello prima di NULL) */	
		while(pktIndex->next != NULL)
			pktIndex = pktIndex->next;

		/* punta il penultimo elemento all'area di memoria appena allocata per il nuovo elemento */
		newPKT = (struct listaPKT *)malloc(sizeof(struct listaPKT));
		pktIndex->next = newPKT;
		for(i=0;i<PKTSIZE;i++)
			newPKT->pkt[i] = insPKT[i];
		newPKT->portaPKT = portapacchetto;
		/* e punta l'attuale penultimo elemento a NULL (ossia l'ultimo elemento) */
		newPKT->next = NULL;
	}
	else /* altrimenti crea il primo elemento e lo fa puntare direttamente dalla testa */
	{
		testa = (struct listaPKT *)malloc(sizeof(struct listaPKT));
		for(i=0;i<PKTSIZE;i++)
			testa->pkt[i] = insPKT[i];
		testa->portaPKT = portapacchetto;
		testa->next = NULL;
	}

	return(testa);

}

/**
  * @brief Rimuove un elemento dalla lista.
  * @param delPkt : ID del pacchetto da eliminare
  * @param *testa : testa della lista
  * @return Restituisce NULL se non vi sono più elementi presenti nella lista, altrimenti una nuova lista senza
  *	    l'elemento passato per parametro.
 */
struct listaPKT *rim_pkt(uint32_t delPKT, struct listaPKT *testa)
{

	/* pktIndex punta all'attuale elemento della lista, mentre prec a quello precedente */
	struct listaPKT *pktIndex, *prec;

	/* Inizializza entrambi i puntatori alla testa della lista */
	prec = pktIndex = testa;

	while(pktIndex != NULL)
		if(pktIndex->pkt[0] == delPKT) break; /* se trova il valore passato, esce */
		else {
			pktIndex = pktIndex->next; /* altrimenti scorre la lista */
			if(prec->next != pktIndex) prec = prec->next; /* prec deve rimanere indietro di un elemento */
		}

	if(pktIndex == NULL) return NULL;
	else	if(pktIndex == prec) { /* se è il primo elemento da eliminare */
			testa = pktIndex->next; /* fa puntare la testa direttamente al secondo elemento */
			free(pktIndex); /* e dealloca la sua area di memoria (del primo) */
		}
		else {
			prec->next = pktIndex->next; /* altrimenti fa puntare l'elemento precedente a quello successivo a punt */
			free(pktIndex); /* e libera la memoria allocata dell'elemento puntato da punt */
		}

	return(testa);

}

/**
  * @brief Ricerca di un pacchetto all'interno della lista.
  * @param ricPKT : ID del pacchetto da cercare
  * @param *testa : puntatore alla testa della lista
  * @return Restituisce NULL se l'elemento ricercato non è presente, altrimenti il puntatore lista all'elemento ricercato.
 */
struct listaPKT *find_id_pkt(uint32_t ricPKT, struct listaPKT *testa)
{

	while(testa != NULL)
		if(testa->pkt[0] == ricPKT) break;
		else testa = testa->next;

	return(testa);

}
