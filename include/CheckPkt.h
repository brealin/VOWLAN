/* CheckPkt.h */

#ifndef __CHECKPKT_H__
#define __CHECKPKT_H__

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>

void init_checkrecvFixed(void);
void init_checkrecvMobile(void);
void set_pkt_recv_at_Mobile(uint32_t idmsg );
void set_pkt_recv_at_Fixed(uint32_t idmsg );
int check_pkt_recv_at_Mobile(uint32_t idmsg );
int check_pkt_recv_at_Fixed(uint32_t idmsg );

void init_checkrecvFixedDelay(void);
void init_checkrecvMobileDelay(void);
void SetpktrecvFixedDelay(uint32_t idletto, uint16_t msecdelay);
void SetpktrecvMobileDelay(uint32_t idletto, uint16_t msecdelay);
uint16_t GetpktrecvFixedDelay(uint32_t idletto);
uint16_t GetpktrecvMobileDelay(uint32_t idletto);


#endif /* __CHECKPKT.H__ */


