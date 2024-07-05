#ifndef	project_h
#define	project_h

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	"pico/stdlib.h"
#include	"pico/unique_id.h"
#include	"hardware/gpio.h"
#include	"hardware/sync.h"
#include	<hardware/flash.h>
#include	"hardware/structs/ioqspi.h"
#include	"hardware/structs/sio.h"

// WIFI COUNTRY
#define	PROJECT_WIFI_COUNTRY	CYW43_COUNTRY_TURKEY


#define	DEBUG	1
/*
#define	VERSION_MAJOR	1
#define	VERSION_MINOR	0
*/
#include	"version.h"

#define	RESET_COUNTER	1000000
#define	RESET_BLINKER	100000
#define	BOOTSEL_COUNTER	3000000
#define	BOOTSEL_BLINKER	300000

typedef	struct {
	char			magic[128];
	char			name[64];
	int			lcdon;
	int			analogon;
	char			hostadr[32];
	int			hostport;
	uint8_t		aps[4][3][32];
	uint64_t		runcount;
	int			doupdate;
	int			newSize;
	int			newPos;
	char			firmwarename[128];
} StoredConfig;
typedef	struct {
	char		stored_config[FLASH_SECTOR_SIZE];
	char		flash[1];
} FlashUpdate;
typedef	union {
	StoredConfig	conf;
	FlashUpdate		fu;
} FlashLayout;

#ifdef	main_c
		char		SharedSecret[64] = "canEliffilEnac";
		StoredConfig	config = {
			"pico_ota_4",			// DEGERLERDE DEGISIKLIK YAPINCA BUT STRING'I DE DEGISTIRIN
			"pico_ota",
			1,						// echo on
			1,						// lcd on
			"173.255.229.145",
			//"www.google.com",
			//"192.168.1.24",
			8899,
			{/*
				{
					{0x40, 0x24, 0xb2, 0xbd, 0x6c, 0xcb},
					"",
					"movita568d"
				},
				*/
			{
					"",
					"sparky",
					"23646336"
				},
				{	
					{0x30,0xcc,0x21,0x3e,0x2c,0x30},
					"",
					"1qazoFa1s4B"
				},
				
				{
					"",
					"EIP\xe2\x98\x8e\xef\xb8\x8f", 
					"e0gvbm6pr30k3"
				},
				
				
				{
					"",
					"",
					""
				}

			}
			
		};
#else
extern	StoredConfig	config;
extern	char			SharedSecret[64];
#endif

#endif