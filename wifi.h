#ifndef	wifi_h
#define	wifi_h

#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include <lwip/pbuf.h>
#include <lwip/tcp.h>

#define 	BUF_SIZE 		2048
#define	POLL_TIME_S		5
#define	TEST_ITERATIONS	10


typedef struct TCP_CLIENT_T_ {
	struct tcp_pcb	*tcp_pcb;
	ip_addr_t		remote_addr;
	uint8_t		buffer[BUF_SIZE];
	uint8_t		senddata[BUF_SIZE];
	int			buffer_len;
	int			sent_len;
	bool			complete;
	int			run_count;
	bool			connected;
} TCP_CLIENT_T;


#ifdef	wifi_c
		int				current_ap = -1;
		TCP_CLIENT_T	tcpc;
#else
extern	int				current_ap;
#endif

void	LoopWifi(void);

#endif