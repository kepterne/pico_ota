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
#include	"hardware/watchdog.h"
#include	"hardware/rtc.h"
#include	"pico_sleep.h"

#include	"pico/cyw43_arch.h"

int	anok = 2;

static void sleep_callback(void) {
    printf("RTC woke us up\n");
    watchdog_reboot(0, 0, 0);
    sleep_ms(1000);
}

void	__no_inline_not_in_flash_func(update_fw)(uintptr_t	saddr, uintptr_t	daddr, int		sz) {
	uint32_t 	ints;
	int	osz = sz;
	if (sz % FLASH_SECTOR_SIZE) {
		sz -= sz % FLASH_SECTOR_SIZE;
		sz += FLASH_SECTOR_SIZE;
	}
	uint32_t	pos = 0;
	char		buf[FLASH_SECTOR_SIZE];

	char		md5txt[64];
	uint8_t	md5[32];
	md5_buffer((char *) (saddr + XIP_BASE), osz, md5);
	md5_digest_string(md5, md5txt);
	printf("MD5 NEW %d %s\r\n", osz, md5txt);
	uint32_t	sz2 = sys.size;
	md5_buffer((char *) (XIP_BASE), sz2, md5);
	md5_digest_string(md5, md5txt);
	printf("MD5 CURRENT %u %s\r\n", sz2, md5txt);
	printf("FUNCTION ADDRESS %p\r\n", update_fw);

	printf("DONE MODE 1---\r\n");
	daddr = 0;

	*((uint32_t *) 0x40058000) &= ~(1<<30);
	ints = save_and_disable_interrupts();
	int	z = 0;
	for (pos = 0 ; pos < sz; pos += FLASH_SECTOR_SIZE) {
		memcpy(buf, (char *) (saddr + XIP_BASE), FLASH_SECTOR_SIZE);

		flash_range_erase(daddr, FLASH_SECTOR_SIZE);
		for (int j = 0; j < 16; j++) {
			flash_range_program(daddr + j * 256, buf + j * 256, 256); // FLASH_SECTOR_SIZE);
			
		}
		watchdog_update();
		///restore_interrupts (ints);
		//	z = memcmp((char *) (daddr + XIP_BASE),  buf, FLASH_SECTOR_SIZE) == 0;
		//	if (z != 0) {
		//		printf("WRITE %d %u\r\n", z, daddr);
		//	}
		//	watchdog_update();
		//	ints = save_and_disable_interrupts();
		saddr += FLASH_SECTOR_SIZE;
		daddr += FLASH_SECTOR_SIZE;
	}
	//ints = save_and_disable_interrupts();
	//flash_flush_cache(); // Note this is needed to remove CSn IO force as well as cache flushing 
     	//flash_enable_xip_via_boot2();
//	md5_buffer((char *) (XIP_BASE), sz2, md5);
//	md5_digest_string(md5, md5txt);

	restore_interrupts (ints);
//	printf("MD5 FINAL %d %s %d\r\n", osz, md5txt, z);
//	watchdog_enable(0, 0);
//	watchdog_reboot(0, 0, 0);
	*((uint32_t *) 0x40058000) |= (1<<30);

	printf("DONE MODE 2---\r\n");
	watchdog_reboot(0, 0, 0);
	for ( ; ; ) tight_loop_contents();

}

void	ProcessFields(TCP_C *tc, char *p) {
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

TCP_C	ServerConnection;
void	alarm_callback(void) {
	printf(" <<<<<<< ALARM >>>>>>>\r\n");
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
			struct	tm *t;
			sscanf(p3, "%lu", &ts);
			ts += 3 * 3600;	// TURKISH TZ FIX
			sys.sBaseTime = ts;
			sys.sStartTime = sys.seconds;
			
			datetime_t dt;
			bzero(&dt, sizeof(dt));
			
			t = localtime(&ts);
			dt.year = t->tm_year + 1900;
			dt.month = t->tm_mon + 1;
			dt.day = t->tm_mday;
			dt.dotw = t->tm_wday;
			dt.hour = t->tm_hour;
			dt.min = t->tm_min;
			dt.sec = t->tm_sec;
			rtc_init();
			int z = rtc_set_datetime(&dt) != 0;
			
			printf("@ <%d> %d TIME SYNCED: %04d %02d %02d - %02d %02d %02d\r\n",
				z,
				t->tm_wday,
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
	case CMD_TCP_BINARY: {
		uint32_t 	ints;
		TCP_C	*tc = (TCP_C *) p1;
		uintptr_t	addr = new_flash_addr;
		addr += tc->binary_off;

		printf("BINARY DATA %u %u\r\n", tc->binary_off, config.newPos);
		if (config.newPos == tc->binary_off) {
			printf("PROGRAMMING FLASH %p\r\n", addr);
			if (memcmp((char *) (addr + XIP_BASE), tc->binary, FLASH_SECTOR_SIZE)) { 
				ints = save_and_disable_interrupts();
				
				flash_range_erase(addr, FLASH_SECTOR_SIZE);
				flash_range_program(addr, tc->binary, FLASH_SECTOR_SIZE);
				restore_interrupts (ints);
				printf("FLASH PROGRAMMED \r\n");
				if (memcmp((char *) (addr + XIP_BASE), tc->binary, FLASH_SECTOR_SIZE)) {
					printf("FLASH PROGRAMMING FAILED %p\r\n", addr);
				}
			} else {
				printf("FLASH NOT CHANGED %p\r\n", addr);
			}
			if (tc->binary_size != FLASH_SECTOR_SIZE) {
				config.newPos += tc->binary_size;
				config.doupdate = 3;
			} else {
				config.newPos += FLASH_SECTOR_SIZE;
				config.doupdate = 1;
			}
			sys.saveconfig = 1;
		}
	}
	break;
	case CMD_TCP_DATA: {
		TCP_C	*tc = (TCP_C *) p1;
		int		l = (int) p3;
		char	*p = p2;
		sys.last_read = sys.seconds;
		printf("TCP DATA < %3d:[%s]\r\n", l, p);
		if (strncasecmp(p, "+SERVER: ", 9))
			return;
		p += 9;
		l -= 9;
		if (strncmp(p, "DWL ", 4) == 0) {
			uint32_t	pos, size;
			if (sscanf(p + 4, "%u %u", &pos, &size) == 2) {
				tc->binary_off = pos;
				tc->binary_size = size;
				tc->binary_p = 0;
				tc->binary[0] = 0;
				return;
			}
 		}
		if (*p == '~') {
			ProcessFields(tc, p);
		} else if (strncmp(p, "PING ", 5) == 0) {
			uint64_t		i;
			int			l = 0;
			if ((i = get64(p + 5))) {
				((uint32_t *) &i)[0] ^= 0x17891789l;
				((uint32_t *) &i)[1] ^= 0x29091999l;
				l = sprintf(tc->senddata, "\nPING: %" PRIu64 "\n", i);
				tcpc_send(tc, tc->senddata, l);
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
			int l = sprintf(tc->senddata, "HELO: %s %s.%s ~name(%s) type(%s) uptime(%llu) id(%s) ver( %s ) wifi(%s)~\n", challenge, sys.id, sys.flashid,
				config.name,
				"PICO_W",
				sys.seconds, 
				sys.id,
				sys.version,
				sys.access_point
			);
			tcpc_send(tc, tc->senddata, l);
		}
	}
	break;
	case CMD_WIFI_CONNECTING: {
		uint8_t	*pp = (uint8_t *) p3;
		printf("Connecting to : \"%s\" | %02x:%02x:%02x:%02x:%02x:%02x : \"%s\"\r\n", p1,
		pp[0], pp[1], pp[2], pp[3], pp[4], pp[5],
		p2
		);
		
	}
	break;

	case CMD_WIFI_CONNECTED: {
		printf("Connected to : \"%s\" ip: %s, gw: %s\r\n", p1, p2, p3);
		sys.last_read = sys.seconds;
		
		connect_to_host(&ServerConnection, config.hostadr, config.hostport);
	}
	break;
	case CMD_WIFI_TICK: {
		printf("W TICK %04X %d\r\n", ServerConnection.state, config.doupdate );
		float vcc = 0;
		uint32_t vccu = 0;
		if (vccu == 0) {
			adc_init();
			adc_gpio_init(29);
		}

		adc_select_input(3);
		vccu = adc_read();
		vcc = vccu * 3.0 * 3.3f / (1 << 12);
		int zz = sprintf(ServerConnection.senddata, "~vcc(%4.2f) vccu(%u)~\r\n", vcc, vccu);
		tcpc_send(&ServerConnection, ServerConnection.senddata, zz);
		if (ServerConnection.state == TCPS_DISCONNECTED)
			connect_to_host(&ServerConnection, config.hostadr, config.hostport);
		else if (ServerConnection.state == TCPS_CONNECTED) {
			if (config.doupdate == 1) {
				config.doupdate = 2;
				
				int l = sprintf(ServerConnection.senddata, "~psize(%u) poff(%u)~\r\n", FLASH_SECTOR_SIZE, config.newPos);
				tcpc_send(&ServerConnection, ServerConnection.senddata, l);
			} else if (config.doupdate == 3) {
				uint32_t	dw = config.newPos;
				printf("FLASHING %p %p %u\r\n", new_flash_addr, (char *) XIP_BASE, config.newPos);
				config.newPos = 0;
				config.doupdate = 0;
				config.magic[0] = 0;
				SaveConfig(&config);
				update_fw(new_flash_addr, XIP_BASE, dw);
			}
		}
	}
	break;
	case CMD_WIFI_DISCONNECTED: {
		printf("Disconnected from : \"%s\"\r\n", p1);

	}
	break;

	case CMD_TCP_NOT_RESOLVING: {
		TCP_C	*tc = (TCP_C *) p1;
		printf("NOT Resolved : \"%s\"\r\n", tc->hostname);
	}
	case CMD_TCP_RESOLVED: {
		TCP_C	*tc = (TCP_C *) p1;
		uint8_t	*p = (uint8_t *) &tc->addr;
		printf("Resolved : \"%s\" -> %d.%d.%d.%d\r\n", tc->hostname,
			p[0], p[1], p[2], p[3]
		);
		
	}
	break;
	case CMD_TCP_NOT_RESOLVED: {
		TCP_C	*tc = (TCP_C *) p1;
		printf("NOT Resolved : \"%s\"\r\n", tc->hostname);
	}
	break;
	case CMD_TCP_CONNECTED: {
		TCP_C	*tc = (TCP_C *) p1;
		printf("Connected : \"%s\":%d\r\n", tc->hostname, tc->port);
	}
	break;
	case CMD_TCP_DISCONNECTED: {
		TCP_C	*tc = (TCP_C *) p1;
		printf("Disconnected : \"%s\":%d\r\n", tc->hostname, tc->port);
	}
	break;
	
	case CMD_UART_DATA: {
		int		idx, val;
		int		segs = (int) p4;
		char		**p = (char **) p3;
		if (strcmp(p[0], "CLR") == 0) {
			printf("\r\n\x1B[2J");
			ClearPrompt();
		} else if (strcmp(p[0], "ID") == 0) {
			printf("ID: %s\r\n\tVersion:%s\r\n\tf:%p\r\n\tsize:%u, %u\r\n\tc:%llu\r\n\tFS:%u\r\nmagix: %s\r\n", sys.id, sys.version, flash_start, sys.size, sys.size/FLASH_PAGE_SIZE, config.runcount, sys.flashsize/FLASH_PAGE_SIZE, config.magic);
		} else if (strcmp(p[0], "RESET") == 0) {
			resetPico();
		} else if (strcmp(p[0], "USB") == 0) {
			reset_usb_boot(0, 0);
		} else if (strcmp(p[0], "SLEEP") == 0) {
			time_t	ts;
			struct tm	*t;
			datetime_t	dt;
			int	offs = 10;
			sscanf(p[1], "%u", &offs);
			ts = getTime();
			ts += offs;
			bzero(&dt, sizeof(dt));
			
			t = localtime(&ts);
			
			dt.year = t->tm_year + 1900;
			dt.month = t->tm_mon + 1;
			dt.day = t->tm_mday;
			dt.dotw = t->tm_wday;
			dt.hour = t->tm_hour;
			dt.min = t->tm_min;
			dt.sec = t->tm_sec;

			printf("@ ALARM SET TO: %04d %02d %02d - %02d %02d %02d\r\n",
				t->tm_year + 1900,
				t->tm_mon + 1,
				t->tm_mday,
				t->tm_hour,
				t->tm_min,
				t->tm_sec
			);
			//uart_default_tx_wait_blocking();
			//watchdog_enable(0, 0);
			//watchdog_enable()
			*((uint32_t *) 0x40058000) &= ~(1<<30);
			sleep_run_from_rosc();
			sleep_goto_sleep_until(&dt, sleep_callback);
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
	watchdog_enable(10000, 0);
	for ( ; ; ) {
		loopSys(&sys);
		loop_wifi();
		watchdog_update();
		if (secs != sys.seconds) {
			secs = sys.seconds;
			DrawPrompt();
		}	
	}
}
