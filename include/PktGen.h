/* PktGen.h  */

#ifndef __PKTGEN_H__
#define __PKTGEN_H__

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

typedef struct {
	int tipogenerazionepkt;
	int fd;
} pkt_generator_parameters;

#ifdef __PKTGEN_C__
void *pkt_generator(void *p);
#else
extern void *pkt_generator(void *p);
#endif

#endif


