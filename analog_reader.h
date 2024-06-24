#ifndef	analog_reader_h
#define	analog_reader_h

#define	CLOCK_DIV 		1
#define	NSAMP			(512 * 5)
#define	CAPTURE_CHANNEL	2

#ifdef	analog_reader_c
		int				reading16[5] = {0, 0, 0, 0, 0};
		uint64_t		samples = 0;
		int				__analog_inuse = 0;
#else
extern	int				reading16[5];
extern	uint64_t		samples;

extern	int				__analog_inuse;
#endif

#define	ADC_0		0
#define	ADC_1		1
#define	ADC_2		2
#define	ADC_VCC		3
#define	ADC_TEMP	4

void	core1_analog(void);
float	NTCTemp(int Adc, int RSeries, int RefT, int RefR, int B);
/*
void	polling_analog(void);
*/

int		analog_toggle(void);
void	analog_on();
void	analog_off();

void	analog_resume(void);
void	analog_pause(void);

#endif