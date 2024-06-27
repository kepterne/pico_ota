#define	main_c
#include	<stdint.h>
#include	<stdlib.h>

#include	"project.h"
#include	"common.h"
#include	<time.h>
#include 	"pico/bootrom.h"
#include	"hardware/gpio.h"
#include	"pico/multicore.h"
#include	"analog_reader.h"
#include	"lcd_display.h"
#include	"wifi.h"
#include	"md5.h"
#include	"hardware/adc.h"

#include	"pico/cyw43_arch.h"

int	anok = 2;

void	ProcessFields(TCP_CLIENT_T *tc, char *p) {
	if (*p != '~')
		return;
	char	*name;
	char	*value;
	for (p++; *p != '~' && *p; ) {
		while (*p == 9 || *p == 32)
			p++;
		if (!isalpha(*p))
			return;
		for (name = p++; isalnum(*p); p++);
		if (*p != '(')
			return;
		*(p++) = 0;
		value = p;
		while (*p != ')' && *p)
			p++;
		if (!*p)
			return;
		*(p++) = 0;
		if (sys.cb)
			(*sys.cb)(CMD_PARAM, (char *) tc, name, value, NULL);
	}
}

void	System(uint32_t cmd, char *p1, char *p2, char *p3, char *p4) {
	switch (cmd) {
	case CMD_PARAM: {
		printf("[%s] = [%s]\r\n", p2, p3);
		if (strcasecmp(p2, "firmware") == 0) {
			if (strcmp(config.firmwarename, p3) >= 0) {
				printf("\r\nUPDATE IS NOT REQUIRED %s > %s\r\n", config.firmwarename, p3);
				
			} else {
				printf("\r\nUPDATE IS REQUIRED\r\n");
				strcpy(config.firmwarename, p3);
				config.newPos = 0;
				config.doupdate = 1;
				sys.saveconfig = 1;
			}
		} else if (strcasecmp(p2, "now") == 0) {
			time_t	ts = 0;
			sscanf(p3, "%lu", &ts);
			ts += 3 * 3600;	// TURKISH TZ FIX
			sys.sBaseTime = ts;
			sys.sStartTime = sys.seconds;
			struct tm	*t;
			t = localtime(&ts);
			printf("@ TIME SYNCED: %04d %02d %02d - %02d %02d %02d\r\n",
				t->tm_year + 1900,
				t->tm_mon + 1,
				t->tm_mday,
				t->tm_hour,
				t->tm_min,
				t->tm_sec
			);
		} else if (strcasecmp(p2, "unow") == 0) {
			uint32_t	ts;
			sscanf(p3, "%lu", &ts);
			sys.usOffset = ts / 1000;
			struct tm	*t;
			ts = getTime();
			t = localtime((time_t *) &ts);
			printf("@ T: %04d %02d %02d - %02d %02d %02d\r\n",
				t->tm_year + 1900,
				t->tm_mon + 1,
				t->tm_mday,
				t->tm_hour,
				t->tm_min,
				t->tm_sec
			);
		}
	}
	break;
	case CMD_TCP_DATA: {
		TCP_CLIENT_T	*tc = (TCP_CLIENT_T *) p1;
		int		l = (int) p3;
		char	*p = p2;
		sys.last_read = sys.seconds;
		printf("TCP DATA < %3d:[%s]\r\n", l, p);
		if (strncasecmp(p, "+SERVER: ", 9))
			return;
		p += 9;
		l -= 9;
		if (*p == '~') {
			ProcessFields(tc, p);
		} else if (strncmp(p, "PING ", 5) == 0) {
			uint64_t		i;
			int			l = 0;
			if ((i = get64(p + 5))) {
				((uint32_t *) &i)[0] ^= 0x17891789l;
				((uint32_t *) &i)[1] ^= 0x29091999l;
				l = sprintf(tc->senddata, "\nPING: %" PRIu64 "\n", i);
			}
			if (config.doupdate) {
				printf("\r\nUPDATE IS REQUESTED %d\r\n", l);
				printf("\nGET %d %d\n", config.newPos, FLASH_PAGE_SIZE);
				sprintf(tc->senddata + l, "\nGET %d %d\n", config.newPos, FLASH_PAGE_SIZE);
			}
		} else if (strncmp(p, "KIMO ", 5) == 0) {
			char  challenge[64];
			uint8_t md5[32];
			p += 5;
			sscanf(p, "%s", challenge);
			strcat(challenge, SharedSecret);
			md5_buffer((uint8_t *) challenge, strlen(challenge), md5);
			md5_digest_string(md5, challenge);
		//	up(" !------------- CHALLENGE -------------! ");
		//	upr(challenge);
			sprintf(tc->senddata, "HELO: %s %s.%s ~name(%s) type(%s) uptime(%llu) id(%s) ver( %s ) wifi(%s)~\n", challenge, sys.id, sys.flashid,
				config.name,
				"PICO_W",
				sys.seconds, 
				sys.id,
				sys.version,
				config.aps[current_ap][0]
			);
		}
	}
	break;
	case CMD_WIFI_CONNECTING: {
		printf("Connecting to : \"%s\"\r\n", p1);
		if (config.lcdon) {
			lcd_set_cursor(2, 0);
			lcd_string(p1);
		}
	}
	break;

	case CMD_WIFI_CONNECTED: {
		printf("Connected to : \"%s\" ip: %s\r\n", p1, p2);
		sys.last_read = sys.seconds;
		if (config.lcdon) {
			lcd_set_cursor(3, 0);
			lcd_string(p2);
		}
	}
	break;

	case CMD_WIFI_DISCONNECTED: {
		printf("Disconnected from : \"%s\"\r\n", p1);
	}
	break;
	case CMD_UART_DATA: {
		int		idx, val;
		int		segs = (int) p4;
		char		**p = (char **) p3;
		if (strcmp(p[0], "DMA") == 0) {
			int	i = analog_toggle();
			printf("ANALOG READER IS %s\r\n", i ? "ON" : "OFF");
			config.analogon = i;
			sys.saveconfig = 1;
		} else if (strcmp(p[0], "LCD") == 0) {
			config.lcdon = lcd_toggle();
			sys.saveconfig = 1;
			printf("LCD IS %s\r\n", config.lcdon  ? "ON" : "OFF");		
		} else if (strcmp(p[0], "CLR") == 0) {
			printf("\r\n\x1B[2J");
			ClearPrompt();
		} else if (strcmp(p[0], "ID") == 0) {
			printf("ID: %s\r\n\tVersion:%s\r\n\tf:%p\r\n\tsize:%u, %u\r\n\tc:%llu\r\n\tFS:%u\r\n", sys.id, sys.version, flash_start, sys.size, sys.size/FLASH_PAGE_SIZE, config.runcount, sys.flashsize/FLASH_PAGE_SIZE);
		} else if (strcmp(p[0], "RESET") == 0) {
			resetPico();
		} else if (strcmp(p[0], "USB") == 0) {
			reset_usb_boot(0, 0);
		} 
	}
	break;
	case CMD_BUTTON_PRESS: {
			uint32_t	d;
			d = (uint32_t) p1;
			printf("\r\nBUTTON PRESS %u\r\n", d);
		}
	break;

	case CMD_CONFIG_STORE: 
			analog_pause();
			printf("CONFIG WILL BE STORED\r\n");
	break;
	case CMD_CONFIG_STORED: 
			analog_resume();
			printf("CONFIG STORED\r\n");
	break;
	case CMD_PROGRAM_INIT: {
			ClearPrompt();
			DrawPrompt();
			GotoCursor(1, 2);
			
			printf("\r\nPROGRAM INIT\r\n");
		}
	break;
	}
}

int	main(void) {
	uint64_t	secs = 0;
	initSys(&sys, System);
	for ( ; ; ) {
		loopSys(&sys);
		loop_wifi();
		if (secs != sys.seconds) {
			secs = sys.seconds;
			DrawPrompt();
		}	
	}
}
