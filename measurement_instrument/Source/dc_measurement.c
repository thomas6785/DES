#include <ADUC841.H>	// special function register definitions
#include "typedef.h"	// variable type definitions
#include "main.h"

void setup_dc_measure(void) {
	// Configure ADC
	ADCCON1 = (1 << MD1_pos)    | // Operating mode of ADC
	          (0 << EXT_REF_pos)| // External reference
	          (0 << CK1_pos)    | // ADC clock divide bits
	          (0 << CK0_pos)    | // ADC clock divide bits
	          (3 << AQ_pos)     | // number of ADC clock cycles
	          (1 << T2C_pos)    | // trigger ADC conversion on timer 2 overflow
	          (0 << EXC_pos);     // external trigger off
						// TODO comments explaining each of these config bits

	// Enable ADC on channel 0
	ADCCON2 = (0 << ADCI_pos)   | // 0: clear ADC interrupt flag
	          (0 << DMA_pos)    | // 0: not using DMA
	          (0 << CCONV_pos)  | // 0: disable continuous conversion
	          (0 << SCONV_pos)  | // 0: no 'single conversion' needed either, we are using timer 2 instead to trigger conversions at regular intervals
	          (0 << CS_pos);      // select channel 0 (AIN0)
}
