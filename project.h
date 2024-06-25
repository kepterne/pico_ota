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
	int			echo;
	int			lcdon;
	int			analogon;
	char			hostadr[32];
	int			hostport;
	char			aps[4][2][32];
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
			"pico_ota_1",			// DEGERLERDE DEGISIKLIK YAPINCA BUT STRING'I DE DEGISTIRIN
			"pico_ota",
			1,						// echo on
			1,						// lcd on
			1,
			"173.255.229.145",
			//"192.168.1.24",
			8899,
			{
				
				{
					"sparky",
					"23646336"
				},
				{
					"EIP\xe2\x98\x8e\xef\xb8\x8f", "e0gvbm6pr30k3"
				},
				{	
					"alphagolf-2.4",
					"1qazoFa1s4B"
				},
				{
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