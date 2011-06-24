/**
 *  @file LBlist.h
 *  @author Vincenzo Ferrari - Barbara Iadarola - Luca Giuliano
 *  @brief File header di liste
 *  @note Questo file contiene gli header delle funzioni per lavorare sulle liste.
 */

#ifndef __LBLISTE_H__
#define __LBLISTE_H__


#include "LB.h"

#ifdef __LBLISTE_C__
#define LBLISTE_EXTERN
#else
#define LBLISTE_EXTERN extern
#endif

/* Struttura delle liste per i pacchetti */
struct listaPKT {
	uint32_t pkt[PKTSIZE];
	uint16_t portaPKT; /* porta a cui è stato inviato il pacchetto */
	struct listaPKT *next;
};

LBLISTE_EXTERN struct			listaPKT *ins_pkt(uint32_t *insPKT, struct listaPKT *testa, uint16_t portapacchetto);
LBLISTE_EXTERN struct			listaPKT *rim_pkt(uint32_t delPKT, struct listaPKT *testa);
LBLISTE_EXTERN struct			listaPKT *find_id_pkt(uint32_t ricPKT, struct listaPKT *testa);

#endif
