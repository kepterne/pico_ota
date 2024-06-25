#define	common_c
#include	"pico/bootrom.h"
#include	"hardware/watchdog.h"
#include	<hardware/flash.h>
#include	"hardware/adc.h"

#include	"project.h"
#include	"common.h"
#include	"wifi.h"

uint64_t	get64(char *p) {
	uint64_t	r = 0;
	for ( ; *p; p++) {
		if (*p >= '0' && *p <= '9')
			r = (r * 10) + *p - '0';
	}
	return r;
}


bool __no_inline_not_in_flash_func(get_bootsel_button)() {
    const uint CS_PIN_INDEX = 1;

    // Must disable interrupts, as interrupt handlers may be in flash, and we
    // are about to temporarily disable flash access!
    uint32_t flags = save_and_disable_interrupts();

    // Set chip select to Hi-Z
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    // Note we can't call into any sleep functions in flash right now
    for (volatile int i = 0; i < 1000; ++i);

    // The HI GPIO registers in SIO can observe and control the 6 QSPI pins.
    // Note the button pulls the pin *low* when pressed.
    bool button_state = !(sio_hw->gpio_hi_in & (1u << CS_PIN_INDEX));

    // Need to restore the state of chip select, else we are going to have a
    // bad time when we return to code in flash!
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    restore_interrupts(flags);

    return button_state;
}

time_t	getTime(void) {
	time_t	t;
	if (!sys.tstart)
		return 0;
	return (sys.unow - sys.ustart - sys.tstart * 1000000 + sys.tbase * 1000000 + sys.toff) / 1000000;
	
}

void	GetBoardID(char *p) {
	int		l = 0;

	pico_unique_board_id_t board_id;
    pico_get_unique_board_id(&board_id);

    for (int i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; ++i) {
        l += sprintf(p + l, "%02x", board_id.id[i]);
    }
}

extern char __flash_binary_end;

void	SaveConfig(StoredConfig *s) {
	uint32_t 	ints;
	int			sz = sizeof(config);
	if (sys.cb)
		(*sys.cb)(CMD_CONFIG_STORE, (char *) &config, NULL, NULL, NULL);
	ints = save_and_disable_interrupts();
	if (sz % FLASH_SECTOR_SIZE)
		sz = sz + FLASH_SECTOR_SIZE - (sz % FLASH_SECTOR_SIZE);
	flash_range_erase(flash_addr, sz);
	sz = sizeof(config);
	if (sz % FLASH_PAGE_SIZE)
		sz = sz + FLASH_PAGE_SIZE - (sz % FLASH_PAGE_SIZE);
	flash_range_program(flash_addr, (char *) &config, sz);
	restore_interrupts (ints);
	if (sys.cb)
		(*sys.cb)(CMD_CONFIG_STORED, (char *) &config, NULL, NULL, NULL);
}

void	UpdateConfig(StoredConfig *s) {
	StoredConfig	*sc;
	sc = (StoredConfig *) flash_start;
	if (memcmp(sc, &config, sizeof(config))) {
		SaveConfig(&config);
	}
}

uint storage_get_flash_capacity(void) {
    uint8_t txbuf[4] = {0x9f};
    uint8_t rxbuf[4] = {0};
    flash_do_cmd(txbuf, rxbuf, 4);
    return 1 << rxbuf[3];
}

void	initSys(SystemConfig *s, void (*f)(uint32_t, char *, char *, char *, char *)) {
	StoredConfig	*sc;
	char	*pp;
#ifndef	DEBUG
	stdio_uart_init_full(uart1, 115200, 4, 5);
#else
	stdio_init_all();
#endif
	sleep_ms(100);
	s->usb_connected = 0;
	s->usb_ack = 0;
	s->ustart = time_us_64();
	s->cb = f;
	s->bootsel = 0;
	s->bootsel_start = 0;
	s->usb_connected = 0;
	GetBoardID(s->id);
	strcpy(s->flashid, s->id);
#ifndef	WIFI
	gpio_init(PICO_DEFAULT_LED_PIN);
   	gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif
	init_version();
	strcpy(s->version, completeVersion);
	
	pp = &__flash_binary_end;
	
	s->size = ((uint32_t) pp) - ((uint32_t) flash_start);
	uintptr_t tt = (uintptr_t) pp;
	if (tt % FLASH_SECTOR_SIZE)
		tt = tt + FLASH_SECTOR_SIZE - (tt % FLASH_SECTOR_SIZE);
	flash_start = (char *) tt;
	flash_addr = tt - (uintptr_t) XIP_BASE;
	uint32_t	ints = save_and_disable_interrupts();
	s->flashsize = storage_get_flash_capacity();
	//s->size = ((uint32_t) pp) - ((uint32_t) flash_start);
	restore_interrupts (ints);
	sc = (StoredConfig *) flash_start;
	
	uint			sz;
	sz = sizeof(config);
	if (sz % FLASH_PAGE_SIZE)
		sz = sz + FLASH_PAGE_SIZE - (sz % FLASH_PAGE_SIZE);
	new_flash_addr = flash_addr + sz;
	if (strcmp(sc->magic, config.magic))
		SaveConfig(&config);
	else
		memcpy(&config, flash_start, sizeof(config));
	config.runcount++;
	UpdateConfig(&config);
	if (s->cb)
		(*s->cb)(CMD_PROGRAM_INIT, (char *) s, (char *) &config, NULL, NULL);
}

void	resetPico(void) {
	//initSys(&sys, sys.cb);
	//sys.usb_ack = 0;
	watchdog_reboot(0, 0, 0);
	sleep_ms(200);
}
extern	int	bootsel_button;

void	LoopButton(SystemConfig *s) {
	int		r =  get_bootsel_button();	
	uint64_t	d;
	if (s->bootsel) {
		int	k = 0;
		d = s->unow - s->bootsel_start;
		if (d >= BOOTSEL_COUNTER) {
			k = ((d - BOOTSEL_COUNTER)/BOOTSEL_BLINKER) & 1;
		} else if (d >= RESET_COUNTER) {
			k = ((d - RESET_COUNTER) / RESET_BLINKER) & 1;
		} else
			k = 0;
		#ifndef	WIFI
			gpio_put(PICO_DEFAULT_LED_PIN, (k ^ 1) ^ PICO_DEFAULT_LED_PIN_INVERTED);
		#else
			cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, k ^ 1);	
		#endif
	}
	if (r != s->bootsel) {	
		s->bootsel = r;
		if (r) {
			s->bootsel_start = s->unow;
		} else {
			#ifndef	WIFI
				gpio_put(PICO_DEFAULT_LED_PIN, 0 ^ PICO_DEFAULT_LED_PIN_INVERTED);
			#else
				cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
			#endif
			if (d > BOOTSEL_COUNTER) {
				reset_usb_boot(0, 0);
				sleep_ms(100);
			} else if (d > RESET_COUNTER) 
				resetPico();
			else if (s->cb)
				(*s->cb)(CMD_BUTTON_PRESS, (char *) ((int) ((d / 1000) & 0xffff)), NULL, NULL, NULL);
		}
	}
}

int	TouchLoop(int GPIN) {
	int		i = 0;
	static int	z = 0, zz = 0;

	gpio_init(GPIN);
	gpio_set_dir(GPIN, 1);
	gpio_put(GPIN, 1);
	gpio_init(GPIN);
	gpio_set_dir(GPIN, 0);

	for ( ; i < 20; i++) {
		if (gpio_get(GPIN)) {
			break;
		}
	}
	
	if (i == 20) {
		z++;
		zz = 0;
		if (z > 60) {
			if (!bootsel_button) {
				bootsel_button =  1;
				zz = 0;
			}
		}
	} else {
		zz += (i == 0);
		if (zz > 90) {
			if (bootsel_button) {
				bootsel_button = 0;
				z = 0;
			}
		}
		z = 0;
	} 
}


#ifdef DEBUG
char		line[512];
int		linec = 0;
int		readon = 0;

/*
void  processLine(char *p, int l) {
	int		idx;
//	printf("L %d:%s\r\n", l, p);
	if (sscanf(p, "ECHO %d", &config.echo) == 1) {
		printf("\r\nECHO: %s\r\n", config.echo ? "ON" : "OFF");
		UpdateConfig(&config);
	} else if (strcasecmp(p, "ID") == 0) {
		printf("\r\nID: %s v:%s f:%p size:%u fs:%u c:%llu\r\n", 
			sys.id, 
			sys.version, 
			flash_start, 
			sys.size, 
			sys.flashsize, 
			config.runcount);
	}
}
*/
char	*skipspace(char *s) {
	for ( ; *s && (*s == ' ' || *s == 9 || *s == 10); s++);
	return s;
}

char	*skipnonspace(char *s) {
	for ( ; *s && *s != ' ' && *s != 9 && *s != 10; s++)
		*s = toupper(*s);
	return s;
}
char	**split_str(char *s, int *idx) {
	static char		*r[32];
	
	s = skipspace(s);
	for (*idx = 0 ; *s; s = skipspace(s)) {
		r[*idx] = s;
		if (*idx < 31)
			(*idx)++;
		if (*(s = skipnonspace(s)))
			*(s++) = 0;
	}
	r[*idx] = NULL;
	return r;
}
void	input_loop(void) {
	int		c;
	while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {		
		if (!sys.usb_ack) {
		//	printf("\r\n\033[2J%s > ", sys.id);
			if (sys.cb)
				(*sys.cb)(CMD_USB_CONNECTED, NULL, NULL, NULL, NULL);
			sys.usb_ack = 1;
			//Esc[2J printf("\r\n\x1B[2J");
			
		}
		sys.usb_connected = sys.unow;
		if (c == 10) {
			continue;
		}
		
		if (config.echo)
			putchar(c);
		if (c == 13) {
			line[linec] = 0;
			if (linec) {
				if (sys.cb) {
					char	**p;
					int	cnt;
					p = split_str(line, &cnt);
					(sys.cb)(CMD_UART_DATA, line, (char *) linec, (char *) p, (char *) cnt);
				}
			} 
			printf("\r\n%s > ", sys.id);
			linec = 0;
		} else {
			line[linec] = c & 0xFF;
			linec += linec < 511 ? 1 : 0;
		}
	} 
	if (c == PICO_ERROR_TIMEOUT)
		if (sys.usb_connected) {
			if ((sys.unow - sys.usb_connected) > 10000000) {
				sys.usb_connected = 0;
				sys.usb_ack = 0;
				if (sys.cb)
					(*sys.cb)(CMD_USB_DISCONNECTED, NULL, NULL, NULL, NULL);
			}
		}
}

#endif

void	loopSys(SystemConfig *s) {
	s->unow = time_us_64();
	s->seconds = (s->unow - s->ustart) / 1000000;
	/*
	adc_select_input(4);
	uint16_t raw = adc_read();
	const float conversion_factor = 3.3f / (1 << 12);
	float result = raw * conversion_factor;
	float temp = 27 - (result - 0.706)/0.001721;
	if (ABSDIFF(s->internal_temp, temp) > 0.2) {
		s->internal_temp = temp;
		//printf("\r\nTemp = %f C\r\n", temp);  
	}
*/
	LoopButton(s);
#ifdef	DEBUG
	input_loop();
#endif

#ifdef	WIFI
	//if (s->seconds > )
		LoopWifi();
#endif
	if (sys.saveconfig) {
		SaveConfig(&config);
		sys.saveconfig = 0;
		printf("\r\nCONFIG SAVED\r\n");
	}
}

void	init_version(void) {
	unsigned char	version[34] = {
		VERSION_MAJOR + '0',
		'.',
		VERSION_MINOR + '0',
		'-', 'D',
//		'-',
		BUILD_YEAR_CH0, BUILD_YEAR_CH1, BUILD_YEAR_CH2, BUILD_YEAR_CH3,
		'-',
		BUILD_MONTH_CH0, BUILD_MONTH_CH1,
		'-',
		BUILD_DAY_CH0, BUILD_DAY_CH1,
		'T',
		BUILD_HOUR_CH0, BUILD_HOUR_CH1,
		':',
		BUILD_MIN_CH0, BUILD_MIN_CH1,
		':',
		BUILD_SEC_CH0, BUILD_SEC_CH1,
		'\0'
	};
	sprintf(completeVersion, "%d.%d-%s", VERSION_MAJOR, VERSION_MINOR, TIMESTAMP);
	//strcpy(completeVersion, (char *) version);
}
