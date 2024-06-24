#ifndef	common_h
#define	common_h

#define	WIFI
#include <time.h>
#define	ABSDIFF(x, y)	((x) > (y) ? (x) - (y) : (y) - (x))

#define	CMD_PROGRAM_INIT		0x0010
#define	CMD_CONFIG_STORE		0x0020
#define	CMD_CONFIG_STORED		0x0021
#define	CMD_BUTTON_PRESS		0x0030
#define	CMD_UART_DATA			0x0040
#define	CMD_USB_DISCONNECTED	0x0050
#define	CMD_USB_CONNECTED		0x0060
#define	CMD_WIFI_CONNECTING		0x0070
#define	CMD_WIFI_CONNECTED		0x0080
#define	CMD_WIFI_DISCONNECTED	0x00A0

#define	CMD_TCP_DATA			0x00B0
#define	CMD_PARAM				0x00C0

typedef	struct {
	char		id[64];
	char		flashid[64];
	char		version[64];
	uint64_t	unow, ustart, tbase, tstart, toff;
	uint64_t	seconds;
	uint32_t	size, flashsize;
	int		bootsel;
	uint64_t	bootsel_start;
	uint64_t	usb_connected;
	uint8_t	usb_ack;
	float		internal_temp;
	void		(*cb)(uint32_t, char *, char *, char *, char *);
} SystemConfig;

uint64_t	get64(char *p);
bool __no_inline_not_in_flash_func(get_bootsel_button)();
void	initSys(SystemConfig *s, void (*f)(uint32_t, char *, char *, char *, char *));
void	loopSys(SystemConfig *s);
void	GetBoardID(char *p);
void	resetPico(void);
time_t	getTime(void);
void	SaveConfig(StoredConfig *s);
void	UpdateConfig(StoredConfig *s);
int	TouchLoop(int GPIN);

#ifdef	common_c
		char			*flash_start = (char *) XIP_BASE;
		uintptr_t		flash_addr;
		uintptr_t		new_flash_addr;
		SystemConfig	sys;
#else
extern	char			*flash_start;
extern	uintptr_t		flash_addr;
extern	SystemConfig	sys;
#endif

#endif
