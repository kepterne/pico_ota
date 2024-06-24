#define	analog_reader_c

#include	<math.h>

#include	"hardware/adc.h"
#include	"hardware/dma.h"
#include	"pico/multicore.h"

#include	"analog_reader.h"

float	NTCTemp(int Adc, int RSeries, int RefT, int RefR, int B) {
	float Vi = Adc * (3.3 / 4095.0);
	float R = (Vi * RSeries) / (3.3 - Vi);
	float To = 273.15 + RefT; 
	float T =  1 / ((1.0 / To) + ((log(R / RefR)) / B));
	return T - 273.15; 
}
extern	int	anok;

void	core1_analog(void) {
	dma_channel_config 		cfg0, cfg1;
	uint16_t				capture_buf[2][NSAMP];
	int						cur_buf = 0;
	int						channel = 0, selected = 0;
	
	
	adc_init();
	gpio_init(25);
	gpio_set_dir(25, 1);
	gpio_put(25, 1);
	adc_gpio_init(26 + 0);
	adc_gpio_init(26 + 1);
	adc_gpio_init(26 + 2);
//	adc_gpio_init(26 + 3);
    adc_set_temp_sensor_enabled(true);
	adc_select_input(0);
	adc_set_round_robin(1 | 2 | 4 | 8 | 16);
	adc_fifo_setup(
		true,  // Write each completed conversion to the sample FIFO
		true,  // Enable DMA data request (DREQ)
		1,     // DREQ (and IRQ) asserted when at least 1 sample present
		false, // We won't see the ERR bit because of 8 bit reads; disable.
		false   // Shift each sample to 8 bits when pushing to FIFO
	);
	adc_set_clkdiv(CLOCK_DIV);
	
	uint 	dma_chan0 = dma_claim_unused_channel(true);
	uint	dma_chan1 = dma_claim_unused_channel(true);
	cfg0 = dma_channel_get_default_config(dma_chan0);
	cfg1 = dma_channel_get_default_config(dma_chan1);

	channel_config_set_transfer_data_size(&cfg0, DMA_SIZE_16);
	channel_config_set_transfer_data_size(&cfg1, DMA_SIZE_16);
	// Reading from constant address, writing to incrementing byte addresses
	
	channel_config_set_read_increment(&cfg0, false);
	channel_config_set_read_increment(&cfg1, false);

	channel_config_set_write_increment(&cfg0, true);
	channel_config_set_write_increment(&cfg1, true);
	// Pace transfers based on availability of ADC samples
	channel_config_set_dreq(&cfg0, DREQ_ADC);
	channel_config_set_dreq(&cfg1, DREQ_ADC);
	// Chain blocks to each other
	channel_config_set_chain_to(&cfg0, dma_chan1);
	channel_config_set_chain_to(&cfg1, dma_chan0);
	adc_fifo_drain();
	adc_run(false);

	dma_channel_configure(dma_chan0, &cfg0,
		capture_buf[0],   // dst
		&adc_hw->fifo, 	// src
		NSAMP,         	// transfer count
		false           	// start immediately
	);
	dma_channel_configure(dma_chan1, &cfg1,
		capture_buf[1],   // dst
		&adc_hw->fifo, // src
		NSAMP,         // transfer count
		false           // start immediately
	);
	
	dma_channel_start(dma_chan0);
	adc_run(true);
	
	for (samples = 0, cur_buf = 0; !anok; cur_buf ^= 1) {
		dma_channel_wait_for_finish_blocking(cur_buf ? dma_chan1 : dma_chan0);
		dma_hw->ch[cur_buf ? dma_chan1 : dma_chan0].al3_write_addr = (io_rw_32) capture_buf[cur_buf];

		uint32_t	total[5] = {0, 0, 0, 0, 0};
		for (int i = 0; i < NSAMP; i++, samples++) {
			total[i % 5] += capture_buf[cur_buf][i];
		}
		for (int i = 0; i < 5; i++)
			reading16[i] = total[i] / (NSAMP / 5);
		//printf("\r\nDMA %d finished [%d]\r\n", cur_buf, total);
	}
	adc_fifo_drain();
	adc_run(false);
	dma_channel_abort(dma_chan0);
	dma_channel_abort(dma_chan1);
	dma_channel_unclaim(dma_chan0);
	dma_channel_unclaim(dma_chan1);
	
	anok = 2;
}
/*

int				poll_analog = 0;
uint			polling_channels[2];
uint16_t		polling_buf[2][NSAMP];


void	polling_analog(void) {
	static	dma_channel_config 	cfg0, cfg1;
	static	int					cur_buf = 0;
	
	if (!poll_analog) {
		poll_analog = 1;	
		adc_init();
		adc_gpio_init(26 + 3);
		gpio_init(25);
		gpio_set_dir(25, 1);
		gpio_put(25, 1);

		adc_gpio_init(26 + 0);
		adc_gpio_init(26 + 1);
		adc_gpio_init(26 + 2);
		adc_gpio_init(26 + 3);
		gpio_put(25, 1);
		adc_set_temp_sensor_enabled(true);
		
		adc_select_input(0);
		adc_set_round_robin( 1 | 2 | 4 | 8 | 16);
	
		adc_fifo_setup(
			true,  // Write each completed conversion to the sample FIFO
			true,  // Enable DMA data request (DREQ)
			1,     // DREQ (and IRQ) asserted when at least 1 sample present
			false, // We won't see the ERR bit because of 8 bit reads; disable.
			false   // Shift each sample to 8 bits when pushing to FIFO
		);
		adc_set_clkdiv(1);	
		polling_channels[0] = dma_claim_unused_channel(true);
		polling_channels[1] = dma_claim_unused_channel(true);
		cfg0 = dma_channel_get_default_config(polling_channels[0]);
		cfg1 = dma_channel_get_default_config(polling_channels[1]);

		// Reading from constant address, writing to incrementing byte addresses
		channel_config_set_transfer_data_size(&cfg0, DMA_SIZE_16);
		channel_config_set_transfer_data_size(&cfg1, DMA_SIZE_16);
		
		channel_config_set_read_increment(&cfg0, false);
		channel_config_set_read_increment(&cfg1, false);

		channel_config_set_write_increment(&cfg0, true);
		channel_config_set_write_increment(&cfg1, true);
		// Pace transfers based on availability of ADC samples
		channel_config_set_dreq(&cfg0, DREQ_ADC);
		channel_config_set_dreq(&cfg1, DREQ_ADC);

		adc_fifo_drain();
		adc_run(false);
		dma_channel_configure(polling_channels[1], &cfg1,
			polling_buf[1],   // dst
			&adc_hw->fifo, // src
			NSAMP,         // transfer count
			false           // start immediately
		);
		
		dma_channel_configure(polling_channels[cur_buf = 0], &cfg0,
			polling_buf[0],   // dst
			&adc_hw->fifo, 	// src
			NSAMP,         	// transfer count
			true           	// start immediately
		);
		//dma_channel_start(polling_channels[cur_buf = 0]);
		adc_run(true);
	}	
	if (dma_channel_is_busy(polling_channels[cur_buf]))
		return;
	
	adc_fifo_drain();
	adc_run(false);
	adc_fifo_drain();
	adc_select_input(0);

		gpio_put(25, 0);
		gpio_put(25, 1);
		adc_set_round_robin( 1 | 2 | 4 | 8 | 16);
	dma_channel_configure(polling_channels[cur_buf ^ 1], cur_buf ? &cfg0 : &cfg1,
		polling_buf[cur_buf ^ 1],   // dst
		&adc_hw->fifo, 	// src
		NSAMP,         	// transfer count
		true           	// start immediately
	);
	
	adc_run(true);
	uint32_t	total[5] = {0, 0, 0, 0, 0};
	
	for (int i = 0; i < NSAMP; i++, samples++) {
		total[i % 5] += polling_buf[cur_buf][i];
	}
	for (int i = 0; i < 5; i++)
		reading16[i] = total[i] / (NSAMP / 5);
	cur_buf ^= 1;
	anok = 0;
	//printf("\r\nDMA %d finished [%d]\r\n", cur_buf, total);

}
*/
int	analog_paused = 0;
void	analog_pause(void) {
	if (!__analog_inuse)
		return;
	analog_off();
	analog_paused = 1;
}

void	analog_resume(void) {
	if (!analog_paused)
		return;
	analog_on();
	analog_paused = 0;
}

void	analog_on(void) {
	if (__analog_inuse)
		return;
	anok = 0;
//	multicore_launch_core1(core1_analog);
	__analog_inuse = 1;
}

void	analog_off(void) {
	if (!__analog_inuse)
		return;
	anok = 1;
	while (anok != 2)
		sleep_ms(1);
	anok = 1;
	multicore_reset_core1();
	__analog_inuse = 0;
}

int	analog_toggle(void) {
	if (__analog_inuse)
		analog_off();
	else
		analog_on();
	return __analog_inuse;
}