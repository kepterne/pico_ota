#define	wifi_c

#include	"project.h"
#include	"common.h"
#include	"wifi.h"

#include "pico/stdlib.h"
#include    "lcd_display.h"

#include "lwip/dns.h"
//#include "lwip/pbuf.h"
//#include "lwip/udp.h"

static bool tcp_client_open(void *arg);
void	tcp_client_init(TCP_CLIENT_T *tc);

int		wifi_state = 0;

int	rssi_list[32];

#define		MAX_APS	32
typedef	struct {
	int		use;
	char		ssid[32];
	uint8_t	bssid[6];
	int		rssi;
	int		auth_mode;
	int		channel;
} ap_entry;

ap_entry	ap_list[MAX_APS];


int	_inside = 0;
static err_t tcpc_result(void *arg, int status);

void	init_scan(void) {
	for (int i = 0; i < 32; i++)
		rssi_list[i] = 0;
	for (int i = 0; i < MAX_APS; i++)
		ap_list[i].use = 0;
}

int	find_ap(const cyw43_ev_scan_result_t *r) {
	ap_entry 	*e;
	int	newp = -1;
	for (int i = 0; i < MAX_APS; i++) {
		if (!ap_list[i].use) {
			if (newp < 0)
				newp = i;
		}
		if (strcmp(r->ssid, ap_list[i].ssid))
			continue;
		
		if (memcmp(r->bssid, ap_list[i].bssid, 6))
			continue;
		newp = i;
		break;
	}
	if (newp < 0)
		return 0;
	e = ap_list + newp;
	if (!e->use) {
		memcpy(e->bssid, r->bssid, 6);
		strcpy(e->ssid, r->ssid);
		e->use = 1;
	}
	e->auth_mode = r->auth_mode;
	e->channel = r->channel;
	e->rssi = r->rssi;
	for (int i = 0; i < MAX_APS - 1; i++) {
		ap_entry	ae;
		if (!ap_list[i].use)
			continue;
		for (int j = i + 1; j < MAX_APS; j++) {
			if (!ap_list[j].use)
				continue;
			if (ap_list[j].rssi > ap_list[i].rssi) {
				memcpy(&ae, ap_list + i, sizeof(ap_entry));
				memcpy(ap_list + i, ap_list + j, sizeof(ap_entry));
				memcpy(ap_list + j, &ae, sizeof(ap_entry));
			}
		}
	}
	return 1;
}

static int scan_result(void *env, const cyw43_ev_scan_result_t *result) {
	if (result) {
		find_ap(result);
	}
	return 0;
}
int	apc = 0;

#define	ST_WIFI_INIT		0
#define	ST_WIFI_FAILED		1000
#define	ST_WIFI_SCAN		10
#define	ST_WIFI_SCANNING	11
#define	ST_WIFI_CONNECT		12
#define	ST_WIFI_CONNECTING	13
#define	ST_WIFI_CONNECTED	14

#define	ST_TCP_CONNECT  	15
#define ST_TCP_CONNECTING   16


int    CheckWifi(void) {
    int err = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
		//err = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
    if (err <= 0) {
        wifi_state = ST_WIFI_SCAN;
        if (sys.cb)
            (*sys.cb)(CMD_WIFI_DISCONNECTED, config.aps[current_ap][0], NULL, NULL, NULL);
    }
    return err <= 0;
}
void	LoopWifi(void) {
	static		uint64_t seconds = 0, timeout = 0;
	int		err = 0;
	if (wifi_state > 0) {
//		cyw43_arch_poll();
//		sleep_ms(1);
	}
	if (sys.seconds >= seconds) {
		seconds = sys.seconds+1;
	} else {
		if (wifi_state != ST_WIFI_SCANNING || wifi_state != ST_WIFI_CONNECTING)
			goto l1;
	}
	cyw43_wifi_scan_options_t scan_options = {0};
	
	switch (wifi_state) {
	case ST_WIFI_INIT:
		if (cyw43_arch_init()) {
			printf("failed to initialise\n");
			wifi_state = ST_WIFI_FAILED;
			goto l1;
		}
		cyw43_arch_enable_sta_mode();
	case ST_WIFI_SCAN:

		init_scan();
		err = cyw43_wifi_scan(&cyw43_state, &scan_options, NULL, scan_result);
		if (err) {
			printf("Failed to start scan: %d\n", err);
			wifi_state = ST_WIFI_SCAN;
		} else {
			wifi_state = ST_WIFI_SCANNING;
		}
	//	cyw43_arch_poll();
        sleep_ms(1);
	break;
	case ST_WIFI_SCANNING:
		if (!cyw43_wifi_scan_active(&cyw43_state)) {
			printf("scan done\n");
			wifi_state = ST_WIFI_SCAN;
			for (int i = 0; i < 4; i++) {
				if (rssi_list[(i + current_ap + 1) % 4] != 0) {
					current_ap = (i + current_ap + 1) % 4;
					wifi_state = ST_WIFI_CONNECT;
					break;
				}
			}
		}
		if (wifi_state == ST_WIFI_SCAN || wifi_state == ST_WIFI_SCANNING)
			break;
	case ST_WIFI_CONNECT:
	//	printf("connecting to %s\r\n", config.aps[current_ap][0]);
		if (sys.cb)
				(*sys.cb)(CMD_WIFI_CONNECTING, sys.access_point, sys.access_point_pw, NULL, NULL);
		cyw43_arch_wifi_connect_async( config.aps[current_ap][0],  config.aps[current_ap][1], CYW43_AUTH_WPA2_MIXED_PSK);
		wifi_state = ST_WIFI_CONNECTING;
		timeout = sys.seconds + 20;
		//break;
	case ST_WIFI_CONNECTING:
		switch ((err = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA))) {
		case CYW43_LINK_UP:
			wifi_state = ST_WIFI_CONNECTED;
			uint8_t	*adr = (char *) &cyw43_state.netif->ip_addr.addr;
			char	ipadr[32], gwaddr[32];
			sprintf(ipadr, "%d.%d.%d.%d", adr[0], adr[1], adr[2], adr[3]);
			adr = (char *) &cyw43_state.netif->gw.addr;
			sprintf(gwaddr, "%d.%d.%d.%d", adr[0], adr[1], adr[2], adr[3]);
			if (sys.cb)
				(*sys.cb)(CMD_WIFI_CONNECTED, config.aps[current_ap][0], ipadr, gwaddr, NULL);
		
			//printf("connected to %s %s %s\r\n", config.aps[current_ap][0], ipadr, gwaddr);
			break;
		default:
			if (timeout <= sys.seconds || err <= 0) {
				cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
				printf("WIFI FAILED %d %d\r\n", err, seconds - sys.seconds);
				seconds = sys.seconds+1;
				wifi_state = ST_WIFI_SCAN;
			}
		}
		break;
    
	case ST_WIFI_CONNECTED:

        if (CheckWifi())
            break;

        wifi_state = ST_TCP_CONNECT;
		break;
    case ST_TCP_CONNECT:
        if (CheckWifi())
            break;
        tcp_client_init(&tcpc);
        if (tcp_client_open(&tcpc))
       	wifi_state = ST_TCP_CONNECTING;
        break;
    case ST_TCP_CONNECTING:

        if (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP) {
		if (sys.last_read + 10 > sys.seconds) {
			
			break;
		}
		printf("TIMEOUT BREAK\r\n");
	  }
        wifi_state = ST_WIFI_SCAN;
        break;
	}
l1:
	return;
}



// Perform initialisation
void	tcp_client_init(TCP_CLIENT_T *tc) {
	bzero(tc, sizeof(*tc));
    ip4addr_aton(config.hostadr, &tc->remote_addr);
}

static err_t tcp_client_close(void *arg) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    err_t err = ERR_OK;
    if (state->tcp_pcb != NULL) {
        tcp_arg(state->tcp_pcb, NULL);
        tcp_poll(state->tcp_pcb, NULL, 0);
        tcp_sent(state->tcp_pcb, NULL);
        tcp_recv(state->tcp_pcb, NULL);
        tcp_err(state->tcp_pcb, NULL);
        err = tcp_close(state->tcp_pcb);
        if (err != ERR_OK) {
            printf("close failed %d, calling abort\n", err);
            tcp_abort(state->tcp_pcb);
            err = ERR_ABRT;
        }
        state->tcp_pcb = NULL;
    }
    return err;
}

static err_t tcp_client_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    printf("tcp_client_sent %u\n", len);
    state->sent_len += len;

    if (state->sent_len >= BUF_SIZE) {

        state->run_count++;
        if (state->run_count >= TEST_ITERATIONS) {
            tcpc_result(arg, 0);
            return ERR_OK;
        }

        // We should receive a new buffer from the server
        state->buffer_len = 0;
        state->sent_len = 0;
        printf("Waiting for buffer from server\n");
    }

    return ERR_OK;
}

static err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    if (err != ERR_OK) {
        printf("connect failed %d\n", err);
        return tcpc_result(arg, err);
    }
    state->connected = true;
    printf("Waiting for buffer from server\n");
    return ERR_OK;
}

static err_t tcp_client_poll(void *arg, struct tcp_pcb *tpcb) {
    printf("tcp_client_poll\n");
    return tcpc_result(arg, -1); // no response is an error?
}

static void tcp_client_err(void *arg, err_t err) {
	_inside = 1;
    if (err != ERR_ABRT) {
        printf("tcp_client_err %d\n", err);
        tcpc_result(arg, err);
    }
	_inside = 0;
}


err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    if (!p) {
        return tcpc_result(arg, -1);
    }
    
    // this method is callback from lwIP, so cyw43_arch_lwip_begin is not required, however you
    // can use this method to cause an assertion in debug mode, if this method is called when
    // cyw43_arch_lwip_begin IS needed
    cyw43_arch_lwip_check();
    if (p->tot_len > 0) {
        printf("recv %d err %d\n", p->tot_len, err);
        for (struct pbuf *q = p; q != NULL; q = q->next) {
            char    *p = q->payload;
            for (int i = 0; i < q->len; i++, p++) 
                if (*p == 13 || *p == 10) {
                    state->buffer[state->buffer_len] = 0;
                    if (state->buffer_len) {
                        if (sys.cb) {
                            (*sys.cb)(CMD_TCP_DATA, (char *) state, state->buffer, (char *) state->buffer_len, NULL);
                            if (state->senddata[0]) {
                                printf("Writing %d bytes to server\n", state->buffer_len);
                                err_t err = tcp_write(tpcb, state->senddata, strlen(state->senddata), TCP_WRITE_FLAG_COPY);
                                if (err != ERR_OK) {
                                    printf("Failed to write data %d\n", err);
                                    return tcpc_result(arg, -1);
                                }
                                state->senddata[0] = 0;
                            }   
                        }
                    }
                    state->buffer[state->buffer_len = 0] = 0;
                } else {
                    state->buffer[state->buffer_len] = *p;
                    if (state->buffer_len < sizeof(state->buffer) - 1)
                        state->buffer_len++;
                }
            
        }
        // Receive the buffer
        tcp_recved(tpcb, p->tot_len);
    }
    pbuf_free(p);
    return ERR_OK;
}

static bool tcp_client_open(void *arg) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T *) arg;
    printf("Connecting to %s port %u\n", ip4addr_ntoa(&state->remote_addr), config.hostport);
    state->tcp_pcb = tcp_new_ip_type(IP_GET_TYPE(&state->remote_addr));
    if (!state->tcp_pcb) {
        printf("failed to create pcb\n");
        return false;
    }

    tcp_arg(state->tcp_pcb, state);
    //tcp_
    tcp_poll(state->tcp_pcb, tcp_client_poll, POLL_TIME_S * 2);
    tcp_sent(state->tcp_pcb, tcp_client_sent);
    tcp_recv(state->tcp_pcb, tcp_client_recv);
    tcp_err(state->tcp_pcb, tcp_client_err);

    state->buffer_len = 0;

    // cyw43_arch_lwip_begin/end should be used around calls into lwIP to ensure correct locking.
    // You can omit them if you are in a callback from lwIP. Note that when using pico_cyw_arch_poll
    // these calls are a no-op and can be omitted, but it is a good practice to use them in
    // case you switch the cyw43_arch type later.
    cyw43_arch_lwip_begin();
    err_t err = tcp_connect(state->tcp_pcb, &state->remote_addr, config.hostport, tcp_client_connected);
    cyw43_arch_lwip_end();

    return err == ERR_OK;
}

#define	WIFIS_INITIALIZE	0
#define	WIFIS_START_SCAN	1
#define	WIFIS_SCANNING	2
#define	WIFIS_CONNECT	3
#define	WIFIS_CONNECTING	4
#define	WIFIS_CONNECTED	5
#define	TCPS_RESOLVING	6
#define	TCPS_RESOLVED	7
#define	TCPS_NOT_RESOLVED	8
#define	TCPS_CONNECTING	9
#define	TCPS_DISCONNECTED	10

#define	WIFIS_FAILED		0x10000000


static err_t tcpc_close(void *arg) {
    TCP_C *state = (TCP_C *)arg;
    err_t err = ERR_OK;
    if (state->tcp_pcb != NULL) {
	printf("CLEARING PCB\r\n");
        tcp_arg(state->tcp_pcb, NULL);
        tcp_poll(state->tcp_pcb, NULL, 0);
        tcp_sent(state->tcp_pcb, NULL);
        tcp_recv(state->tcp_pcb, NULL);
        tcp_err(state->tcp_pcb, NULL);

	printf("PCB CLEARED\r\n");
        err = tcp_close(state->tcp_pcb);
	  printf("TCP CLOSED\r\n");
        if (err != ERR_OK) {
            printf("close failed %d, calling abort\n", err);
	  printf("TCP ABORTING\r\n");
	      tcp_abort(state->tcp_pcb);
            err = ERR_ABRT;
        }
	    printf("TCP DISCONNECTED\r\n");
        state->tcp_pcb = NULL;
	  state->state = TCPS_DISCONNECTED;
    }
    return err;
}


static err_t tcpc_result(void *arg, int status) {
	err_t	r;
	TCP_C *state = (TCP_C *) arg;
	if (status == 0) {
		printf("test success\n");
	} else {
		printf("testc failed %d\n", status);
//		wifi_state = ST_WIFI_CONNECTED;
	}
//	state->complete = true;
	r = tcpc_close(arg);
	return r;
}

static err_t tcpc_poll(void *arg, struct tcp_pcb *tpcb) {
	err_t	r;
	_inside = 1;
	printf("tcp_client_poll\n");
	r = tcpc_result(arg, -1); // no response is an error?

	_inside = 0;
	return r;
}

static err_t tcpc_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
	TCP_C *state = (TCP_C *) arg;
	_inside = 1;
//    printf("tcp_client_sent %u\n", len);
	_inside = 0;
    return ERR_OK;
}

err_t tcpc_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
	TCP_C *state = (TCP_C *) arg;
	err_t r;
	_inside = 1;
	if (!p) {
		r = tcpc_result(arg, -1);
		_inside = 0;
		return r;
	}

	// this method is callback from lwIP, so cyw43_arch_lwip_begin is not required, however you
	// can use this method to cause an assertion in debug mode, if this method is called when
	// cyw43_arch_lwip_begin IS needed
	cyw43_arch_lwip_check();
	if (p->tot_len > 0) {
		printf("recv %d err %d\n", p->tot_len, err);
		for (struct pbuf *q = p; q != NULL; q = q->next) {
			char    *p = q->payload;
			for (int i = 0; i < q->len; i++, p++) 
				if (*p == 13 || *p == 10) {
					state->line[state->linepos] = 0;
					if (state->linepos) {
						if (sys.cb) {
							(*sys.cb)(CMD_TCP_DATA, (char *) state, state->line, (char *) state->linepos, NULL);
				/*			if (state->senddata[0]) {
								printf("Writing %d bytes to server\n", state->buffer_len);
								err_t err = tcp_write(tpcb, state->senddata, strlen(state->senddata), TCP_WRITE_FLAG_COPY);
								if (err != ERR_OK) {
								printf("Failed to write data %d\n", err);
								return tcpc_result(arg, -1);
								}
								state->senddata[0] = 0;
							}   
				*/
						}
					}
					state->line[state->linepos = 0] = 0;
				} else {
					state->line[state->linepos] = *p;
					if (state->linepos < sizeof(state->line) - 1)
						state->linepos++;
				}

		}
		// Receive the buffer
		tcp_recved(tpcb, p->tot_len);
	}
	pbuf_free(p);
	_inside = 0;
	return ERR_OK;
}

static void tcpc_err(void *arg, err_t err) {
	_inside = 1;
	if (err != ERR_ABRT) {
		printf("tcp_client_err %d\n", err);
		tcpc_result(arg, err);
	}
	_inside = 0;
}
static err_t tcpc_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
	TCP_C *state = (TCP_C *)arg;
	err_t	r;
	_inside = 1;
	if (err != ERR_OK) {
		printf("connect failed %d\n", err);
		r = tcpc_result(arg, err);
		_inside = 0;
		return r;
	}
	state->state = TCPS_CONNECTING;
	printf("Waiting for buffer from server\n");
	_inside = 0;
	return ERR_OK;
}
int	connect_tcp(TCP_C *tc) {
	printf("Connecting...\r\n");
	tc->tcp_pcb = tcp_new_ip_type(IP_GET_TYPE(&tc->addr));
	if (!tc->tcp_pcb) {
		return 1;
	}

	printf("Connecting...1\r\n");
	tcp_arg(tc->tcp_pcb, tc);
	tcp_poll(tc->tcp_pcb, tcpc_poll, POLL_TIME_S * 2);
	tcp_sent(tc->tcp_pcb, tcpc_sent);
	tcp_recv(tc->tcp_pcb, tcpc_recv);
	tcp_err(tc->tcp_pcb, tcpc_err);
	if (!_inside)
		cyw43_arch_lwip_begin();
	err_t err = tcp_connect(tc->tcp_pcb, &tc->addr, tc->port, tcpc_connected);
	if (!_inside)
		cyw43_arch_lwip_end();
	return 0;
}

// Call back with a DNS result
static void tcp_dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg) {
	TCP_C *state = (TCP_C *) arg;
	_inside = 1;
	if (ipaddr) {
		state->addr = *ipaddr;
		state->state = TCPS_RESOLVED;
		if (sys.cb)
			(*sys.cb)(CMD_TCP_RESOLVED, (char *) state, NULL, NULL, NULL);
		connect_tcp(state);	
		//sys.wifi_status = WIFIS_RESOLVED;
		//printf("ntp address %s\n", ipaddr_ntoa(ipaddr));
		//ntp_request(state);
		//connect_tcp(state);
	} else {
		state->state = TCPS_NOT_RESOLVED;
		if (sys.cb)
			(*sys.cb)(CMD_TCP_NOT_RESOLVED, (char *) state, NULL, NULL, NULL);
		
		//sys.wifi_status = WIFIS_FAILED;
		//printf("ntp dns request failed\n");
		//ntp_result(state, -1, NULL);

	}
	_inside = 0;
}
int	connect_to_tcp(TCP_C *tc, char *hostname, int port) {
	bzero(tc, sizeof(*tc));
	strcpy(tc->hostname, hostname);
	tc->port = port;
	if (!ip4addr_aton(hostname, &tc->addr)) {
		tc->state = TCPS_RESOLVING;
		if (sys.cb)
			(*sys.cb)(CMD_TCP_NOT_RESOLVING, (char *) tc, NULL, NULL, NULL);
		if (!_inside)
			cyw43_arch_lwip_begin();
            int err = dns_gethostbyname(tc->hostname, &tc->addr, tcp_dns_found, tc);
            if (!_inside)
			cyw43_arch_lwip_end();
		
	} else {
		if (sys.cb) {
			(*sys.cb)(CMD_TCP_RESOLVED, (char *) tc, NULL, NULL, NULL);	
			connect_tcp(tc);
		}
	}
	return 0;
}

void	loop_wifi(void) {
	if (sys.seconds < 1)
		return;
	switch (sys.wifi_status) {
	case WIFIS_INITIALIZE:
		printf("Initializing wifi...\r\n");
		if (cyw43_arch_init()) {
			printf("failed to initialise\n");
			sys.wifi_status = WIFIS_FAILED;
			break;
		}
		printf("Entering station mode... %d \r\n", sys.wifi_status);
		cyw43_arch_enable_sta_mode();
		sys.wifi_status = WIFIS_START_SCAN;
		printf("Initialized wifi... %d \r\n", sys.wifi_status);
		break;
	case WIFIS_START_SCAN: {
			int err;
			cyw43_wifi_scan_options_t scan_options = {0};
			printf("WiFi Scanning\r\n");
			init_scan();
			err = cyw43_wifi_scan(&cyw43_state, &scan_options, NULL, scan_result);
			if (err) {
				printf("Failed to start scan: %d\n", err);
				sys.wifi_status = WIFIS_FAILED;
			} else {
				printf("WiFi Scan Started\r\n");
				sys.wifi_status = WIFIS_SCANNING;
			}

		}
		break;
	case WIFIS_SCANNING:
		if (!cyw43_wifi_scan_active(&cyw43_state)) {
			int	connect = 1;
			//ClearPrompt();
			printf("\033[2J");
			DrawPrompt();
			GotoCursor(1, 3);
			printf("Scan done\r\n");
			
			for (int i = 0; i < MAX_APS; i++) {
				ap_entry	*result = ap_list + i;
				if (!ap_list[i].use)
					continue;
				printf("%-32s rssi: %4d chan: %3d mac: %02x:%02x:%02x:%02x:%02x:%02x sec: %u\n",
				result->ssid, result->rssi, result->channel,
				result->bssid[0], result->bssid[1], result->bssid[2], result->bssid[3], result->bssid[4], result->bssid[5],
				result->auth_mode);
				for (int j = 0; j < 4; j++) {
					if (strcasecmp(config.aps[j][0], result->ssid))
						continue;
					if (!sys.access_point[0]) {
						strcpy(sys.access_point, result->ssid);
						strcpy(sys.access_point_pw, config.aps[j][1]);
						connect = 1;
					}
				}
			}
			if (connect) {
				sys.wifi_status = WIFIS_CONNECT;
			} else
				sys.wifi_status = WIFIS_START_SCAN;
			printf("scan done\n");
		
		}
		break;
	case WIFIS_CONNECT: {
			if (sys.cb)
				(*sys.cb)(CMD_WIFI_CONNECTING, sys.access_point, sys.access_point_pw, NULL, NULL);
			cyw43_arch_wifi_connect_async(sys.access_point,  sys.access_point_pw, CYW43_AUTH_WPA2_MIXED_PSK);
			sys.wifi_status = WIFIS_CONNECTING;
//			printf("CONNECTING TO : %s : %s\r\n", sys.access_point, sys.access_point_pw);
			sys.wifi_timeout = sys.seconds + 15;
		}
		break;
	case WIFIS_CONNECTING: {
			int	err;
			switch ((err = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA))) {
			case CYW43_LINK_UP:
				sys.wifi_status = WIFIS_CONNECTED;;
				uint8_t	*adr = (char *) &cyw43_state.netif->ip_addr.addr;
				char	ipadr[32], gwaddr[32];
				sprintf(ipadr, "%d.%d.%d.%d", adr[0], adr[1], adr[2], adr[3]);
				adr = (char *) &cyw43_state.netif->gw.addr;
				sprintf(gwaddr, "%d.%d.%d.%d", adr[0], adr[1], adr[2], adr[3]);
				if (sys.cb)
					(*sys.cb)(CMD_WIFI_CONNECTED, sys.access_point, ipadr, gwaddr, NULL);
			
			//	printf("connected to %s %s %s\r\n", config.aps[current_ap][0], ipadr, gwaddr);
				break;
			default:
				if (sys.wifi_timeout <= sys.seconds || err <= 0) {
					cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
					printf("WIFI FAILED %d %d\r\n", err, sys.wifi_timeout - sys.seconds);
					
					sys.wifi_status = WIFIS_START_SCAN;
				}
			}
		}
		break;
	case WIFIS_CONNECTED:
		//printf("NEW TCP\r\n");
		//sys.wifi_status = WIFIS_FAILED;
		break;
	case WIFIS_FAILED:
		break;
	}
	//sleep_ms(1);
}