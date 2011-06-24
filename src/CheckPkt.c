/* CheckPkt.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>

#include "../include/VoIPpkt.h"

typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned long int uint32_t;

#define DURATAAPPL_MIN 10L
#define SECINMIN 60L
#define USEC_IN_SEC 1000000L
#define MAXNUMPKTS ((unsigned long int)( (DURATAAPPL_MIN*SECINMIN*USEC_IN_SEC)/USEC_VOIPPKTLEN) )
#define CHECKSIZE (MAXNUMPKTS/8)

static uint8_t pktrecvMobile[CHECKSIZE];
static uint8_t pktrecvFixed[CHECKSIZE];
static uint16_t *pktrecvMobileDelay;
static uint16_t *pktrecvFixedDelay;

void init_checkrecvFixed(void)
{	memset(pktrecvFixed,0,CHECKSIZE); }

void init_checkrecvMobile(void)
{	memset(pktrecvMobile,0,CHECKSIZE); }

void init_checkrecvFixedDelay(void)
{
	pktrecvFixedDelay=malloc(MAXNUMPKTS*sizeof(uint16_t));
	if(pktrecvFixedDelay==NULL) { fprintf(stderr,"malloc failed. exit\n"); exit(8); }
	memset(pktrecvFixedDelay,0,MAXNUMPKTS);
}

void init_checkrecvMobileDelay(void)
{
	pktrecvMobileDelay=malloc(MAXNUMPKTS*sizeof(uint16_t));
	if(pktrecvMobileDelay==NULL) { fprintf(stderr,"malloc failed. exit\n"); exit(8); }
	memset(pktrecvMobileDelay,0,MAXNUMPKTS);
}

void SetpktrecvFixedDelay(uint32_t idletto, uint16_t msecdelay)
{
	pktrecvFixedDelay[idletto]=msecdelay;
}

void SetpktrecvMobileDelay(uint32_t idletto, uint16_t msecdelay)
{
	pktrecvMobileDelay[idletto]=msecdelay;
}

uint16_t GetpktrecvFixedDelay(uint32_t idletto)
{
	return( pktrecvFixedDelay[idletto] );
}

uint16_t GetpktrecvMobileDelay(uint32_t idletto)
{
	return( pktrecvMobileDelay[idletto] );
}

static void settaBit(uint8_t *pval, int offset)
{
	uint8_t mask;

	mask=1; 
	mask=mask<<offset;
	/* printf("mask %d\n", mask); */
	*pval = *pval | mask;
}

static int set_pkt_recv(uint8_t *ptr, uint32_t idmsg )
{
	int index, offset;
	uint8_t val;

	if(idmsg>=MAXNUMPKTS) { fprintf(stderr,"idmsg too large. exit\n"); exit(8); }
	index=idmsg/8;
	offset=idmsg%8;
	val=ptr[index];
	/* printf("index %d offset %d prima val %d   ", index, offset, val); */
	settaBit(&val,offset);
	ptr[index]=val;
	/* printf("dopo val %d\n", val); */
	return(1);
}

uint8_t getBit(uint8_t val, int offset)
{
	uint8_t mask;

	mask=1; 
	mask=mask<<offset;
	/* mask = ~mask; */
	if( val&mask ) return(1);
	else return(0);
}

/* restituisce il valore del bit, 1 se ricevuto, 0 se non ricevuto */
static int check_pkt_recv(uint8_t *ptr, uint32_t idmsg )
{
	int index, offset;
	uint8_t val;

	if(idmsg>=MAXNUMPKTS) { fprintf(stderr,"idmsg too large. exit\n"); exit(8); }
	index=idmsg/8;
	offset=idmsg%8;
	val=getBit(ptr[index],offset);
	return( (int)val);
}

void set_pkt_recv_at_Mobile(uint32_t idmsg )
{
	set_pkt_recv(pktrecvMobile,idmsg);
}

void set_pkt_recv_at_Fixed(uint32_t idmsg )
{
	set_pkt_recv(pktrecvFixed,idmsg);
}

int check_pkt_recv_at_Mobile(uint32_t idmsg )
{
	return( check_pkt_recv(pktrecvMobile,idmsg) );
}

int check_pkt_recv_at_Fixed(uint32_t idmsg )
{
	return( check_pkt_recv(pktrecvFixed,idmsg) );
}


/*
int main(void)
{
	int i;

	init_checkrecvFixed();
	printf("A1\n");
	init_checkrecvMobile();
	printf("A2\n");
	for(i=0;i<10000L;i++)
		if( check_pkt_recv_at_Fixed(i) == 1 ) 
			printf("CHECK FIXED FAILED %d \n", i);
		else
			printf(".");
	printf("C\n");
	for(i=0;i<10000L;i++)
		if( check_pkt_recv_at_Mobile(i) == 1 ) 
			printf("CHECK MOBILE FAILED %d \n", i);
		else
			printf(".");
	printf("D\n");

	for(i=0;i<10000L;i++)
	{
		init_checkrecvFixed();
		set_pkt_recv_at_Fixed(i);
		if( check_pkt_recv_at_Fixed(i) == 0 ) 
			printf("SET FIXED FAILED %d \n", i);
		else
			printf(".");
	}
	printf("E\n");
	for(i=0;i<10000L;i++)
	{
		init_checkrecvMobile();
		set_pkt_recv_at_Mobile(i);
		if( check_pkt_recv_at_Mobile(i) == 0 ) 
			printf("SET MOBILE FAILED %d \n", i);
		else
			printf(".");
	}
	printf("F\n");
	return(0);

}
*/


