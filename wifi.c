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
	const cyw43_ev_scan_result_t *result = r;
/*
	printf("%-32s rssi: %4d chan: %3d mac: %02x:%02x:%02x:%02x:%02x:%02x sec: %u\n",
		result->ssid, result->rssi, result->channel,
		result->bssid[0], result->bssid[1], result->bssid[2], result->bssid[3], result->bssid[4], result->bssid[5],
		result->auth_mode
	);
	return 0;
*/
		


	for (int i = 0; i < MAX_APS; i++) {
		if (!ap_list[i].use) {
			if (newp < 0)
				newp = i;
		}
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
		
	}
	printf("TCP DISCONNECTED\r\n");
	state->tcp_pcb = NULL;
	state->state = TCPS_DISCONNECTED;
	if (sys.cb) {
		(*sys.cb)(CMD_TCP_DISCONNECTED, (char *) state, NULL, NULL, NULL);
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

err_t	tcpc_send(TCP_C *state, char *data, int len) {
	err_t	err;
	if (!state)
		return 0;
	if (!state->tcp_pcb)
		return 0;
	
	printf("Writing %d bytes to server\n", len);
	tcp_nagle_disable(state->tcp_pcb);
	err = tcp_write(state->tcp_pcb, data, len, TCP_WRITE_FLAG_COPY);
	if (err != ERR_OK) {
		printf("Failed to write data %d\n", err);
		err = tcpc_result(state, -1);
		
		return err;
	} 
	//while (state->tcp_pcb->unsent)
	//	if (tcp_output(state->tcp_pcb) != ERR;
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
		//printf("recv %d err %d\n", p->tot_len, err);
		for (struct pbuf *q = p; q; q = q->next) {
			char    *p = q->payload;
			for (int i = 0; i < q->len; i++, p++) {
				if (state->binary_size) {					
					state->binary[state->binary_p++] = *p;
					if (state->binary_p >= state->binary_size) {
						printf("binary data received %d\r\n", state->binary_size);
						(*sys.cb)(CMD_TCP_BINARY, (char *) state, state->binary, (char *) state->binary_size, (char *) state->binary_off);
						state->binary_size = 0;
						
					}
					
				} else if (*p == 13 || *p == 10) {
					state->line[state->linepos] = 0;
					if (state->linepos) {
						if (sys.cb) {
							(*sys.cb)(CMD_TCP_DATA, (char *) state, state->line, (char *) state->linepos, NULL);
						}
					}
					state->line[state->linepos = 0] = 0;
				} else {
					state->line[state->linepos] = *p;
					if (state->linepos < sizeof(state->line) - 1)
						state->linepos++;
				}
			}
		}
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
	state->state = TCPS_CONNECTED;
	//printf("Waiting for buffer from server\n");
	if (sys.cb) {
		(*sys.cb)(CMD_TCP_CONNECTED, (char *) state, NULL, NULL, NULL);
	}
	_inside = 0;
	return ERR_OK;
}

int	tcp_do_connect(TCP_C *tc) {
	printf("Connecting...\r\n");
	tc->tcp_pcb = tcp_new_ip_type(IP_GET_TYPE(&tc->addr));
	if (!tc->tcp_pcb) {
		return 1;
	}

	printf("Connecting...1\r\n");
	tcp_arg(tc->tcp_pcb, tc);
	tcp_poll(tc->tcp_pcb, tcpc_poll, POLL_TIME_S);
	tcp_poll(tc->tcp_pcb, NULL, 0);
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
static void tcp_dns_callback(const char *hostname, const ip_addr_t *ipaddr, void *arg) {
	TCP_C *state = (TCP_C *) arg;
	_inside = 1;
	if (ipaddr) {
		state->addr = *ipaddr;
		state->state = TCPS_RESOLVED;
		if (sys.cb)
			(*sys.cb)(CMD_TCP_RESOLVED, (char *) state, NULL, NULL, NULL);
		tcp_do_connect(state);
	} else {
		state->state = TCPS_NOT_RESOLVED;
		if (sys.cb)
			(*sys.cb)(CMD_TCP_NOT_RESOLVED, (char *) state, NULL, NULL, NULL);
		
		//state->state = TCPS_DISCONNECTED;
		tcpc_close(state);
	}
	_inside = 0;
}

int	connect_to_host(TCP_C *tc, char *hostname, int port) {
	if (tc->tcp_pcb)
		return 99;
	bzero(tc, sizeof(*tc));
	strcpy(tc->hostname, hostname);
	tc->port = port;
	if (!ip4addr_aton(hostname, &tc->addr)) {
		tc->state = TCPS_RESOLVING;
		if (sys.cb)
			(*sys.cb)(CMD_TCP_NOT_RESOLVING, (char *) tc, NULL, NULL, NULL);
		if (!_inside)
			cyw43_arch_lwip_begin();
            int err = dns_gethostbyname(tc->hostname, &tc->addr, tcp_dns_callback, tc);
            if (!_inside)
			cyw43_arch_lwip_end();
		
	} else {
		if (sys.cb) {
			(*sys.cb)(CMD_TCP_RESOLVED, (char *) tc, NULL, NULL, NULL);	
			tcp_do_connect(tc);
		}
	}
	return 0;
}

int	connect = 0;

void	loop_wifi(void) {
	switch (sys.wifi_status) {
	case WIFIS_INITIALIZE: {
			printf("Initializing wifi...\r\n");
			if (cyw43_arch_init_with_country(PROJECT_WIFI_COUNTRY)) {
				printf("failed to initialise\n");
				sys.wifi_status = WIFIS_FAILED;
				break;
			}
			printf("Entering station mode... %d \r\n", sys.wifi_status);
			cyw43_arch_enable_sta_mode();
			sys.wifi_status = WIFIS_START_SCAN;
			printf("Initialized wifi... %d \r\n", sys.wifi_status);
		}
		break;
		
	case WIFIS_START_SCAN: {
			int err;
			if (sys.seconds < 1)
				break;
			cyw43_wifi_scan_options_t scan_options = {0};
			printf("WiFi Scanning\r\n");
			cyw43_arch_enable_sta_mode();
			init_scan();
			connect = 0;
			bzero(&scan_options, sizeof(scan_options));
			scan_options.scan_type = 1;
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
			sleep_ms(100);
			//ClearPrompt();
			//printf("\033[2J");
			DrawPrompt();
			//GotoCursor(1, 3);
			printf("Scan done\r\n");
			connect = 0;
			cyw43_arch_disable_sta_mode();
			for (int i = 0; i < MAX_APS; i++) {
				ap_entry	*result = ap_list + i;
				if (!ap_list[i].use)
					continue;
				printf("%-32s rssi: %4d chan: %3d mac: %02x:%02x:%02x:%02x:%02x:%02x sec: %u\n",
				result->ssid, result->rssi, result->channel,
				result->bssid[0], result->bssid[1], result->bssid[2], result->bssid[3], result->bssid[4], result->bssid[5],
				result->auth_mode);
				if (!connect)
				for (int j = 0; j < 4; j++) {
					if (config.aps[j][0][0]) {
						/*
						printf("CMP %02x:%02x:%02x:%02x:%02x:%02x ?= %02x:%02x:%02x:%02x:%02x:%02x\r\n",
							config.aps[j][0][0],
							config.aps[j][0][1],
							config.aps[j][0][2],
							config.aps[j][0][3],
							config.aps[j][0][4],
							config.aps[j][0][5],

							result->bssid[0],
							result->bssid[1],
							result->bssid[2],
							result->bssid[3],
							result->bssid[4],
							result->bssid[5]
									
						);
						*/
						if (memcmp(config.aps[j][0], result->bssid, 6))
							continue;
						//printf("matched bssid\r\n");
						memcpy(sys.bssid, result->bssid, 6);
						sys.access_point[0] = 0;
						strcpy(sys.access_point_pw, config.aps[j][2]);
						connect = 1;
						//printf("CONNECTING TO : %s : %s\r\n", result->ssid, sys.access_point_pw);
						
					} else if (config.aps[j][1][0]) {
						if (strcasecmp(config.aps[j][1], result->ssid))
							continue;
						memcpy(sys.bssid, result->bssid, 6);
						strcpy(sys.access_point, result->ssid);
						strcpy(sys.access_point_pw, config.aps[j][2]);
						connect = 1;
						//printf("CONNECTING TO : %s : %s\r\n", sys.access_point, sys.access_point_pw);
						
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
				(*sys.cb)(CMD_WIFI_CONNECTING, sys.access_point, sys.access_point_pw, sys.bssid, NULL);
			//cyw43_wifi_pm(&cyw43_state, 0xa11140);
			cyw43_arch_enable_sta_mode();
			cyw43_arch_wifi_connect_bssid_async("", sys.bssid, sys.access_point_pw, CYW43_AUTH_WPA2_MIXED_PSK);
			sys.wifi_status = WIFIS_CONNECTING;
		//printf("CONNECTING TO : %s : %s\r\n", sys.access_point, sys.access_point_pw);
			sys.wifi_timeout = sys.seconds + 120;
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
			
				break;
			default:
				if (sys.wifi_timeout <= sys.seconds || err < 0) {
					cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
					printf("WIFI FAILED %d %d\r\n", err, (uint32_t) sys.wifi_timeout - sys.seconds);
					cyw43_arch_disable_sta_mode();
					sys.wifi_status = WIFIS_START_SCAN;
				}
			}
		}
		break;
	case WIFIS_CONNECTED: {
		static	uint64_t	secs = 0;
		if (sys.seconds != secs) {
			if (sys.cb)
					(*sys.cb)(CMD_WIFI_TICK, NULL, NULL, NULL, NULL);
			secs = sys.seconds;
		}
		//printf("NEW TCP\r\n");
		//sys.wifi_status = WIFIS_FAILED;
		}
		break;
	case WIFIS_FAILED:
		break;
	}

//	sleep_ms(1);
}