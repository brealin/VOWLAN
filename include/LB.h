/**
 *  @file LB.h
 *  @author Vincenzo Ferrari - Barbara Iadarola - Luca Giuliano
 *  @brief Header delle collezioni di funzioni.
 *  @note Questo file header contiene la dichiarazione delle funzioni utilizzate dai due Load Balancer.
 */

#ifndef __LB_H__
#define __LB_H__

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

#include "Util.h"
#include "VoIPpkt.h"
#include "const.h"

#ifdef __LB_C__
#define LB_EXTERN
#else
#define LB_EXTERN extern
#endif

LB_EXTERN void 		sig_print(int signo);
LB_EXTERN void 		stampa_fd_set(char *str, fd_set *pset);
LB_EXTERN int 		send_udp(uint32_t socketfd, char *buf, uint32_t len, uint16_t port_number_local, char *IPaddr, uint16_t port_number_dest);
LB_EXTERN long int 	compute_delay(uint32_t *buf);
LB_EXTERN void 		send_ping(uint32_t numporte, uint16_t *cfgPorte, int32_t monitordatafd);
LB_EXTERN int 		config_porte(uint32_t *buf, uint32_t numporte, uint16_t *cfgPorte, int index, uint16_t portaAttuale);
LB_EXTERN void 		send_config(uint32_t *cfgPKT, int32_t monitordatafd, uint16_t *cfgPorte, uint32_t numporte);
LB_EXTERN int 		trova_porta(uint16_t portFrom, uint16_t *cfgPorte, int index, uint32_t numporte);
LB_EXTERN void 		initV(uint32_t *vettore);

#endif
