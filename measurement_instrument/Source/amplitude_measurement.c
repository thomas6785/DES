#include <ADUC841.H>	// special function register definitions
#include "typedef.h"	// variable type definitions
#include "main.h"

void setup_amplitude_measure(void) {
	// Configure ADC to measure with frequency 150kHz
	ADCCON1 = (1 << MD1_pos)    | // Operating mode of ADC
	          (0 << EXT_REF_pos)| // External reference
	          (0 << CK1_pos)    | // ADC clock divide bits
	          (0 << CK0_pos)    | // ADC clock divide bits
	          (3 << AQ_pos)     | // number of ADC clock cycles
	          (0 << T2C_pos)    | // Which timer to use
	          (0 << EXC_pos);     // external trigger

	ADCCON2 = (0 << ADCI_pos)   | // ADC interrupt flag
	          (0 << DMA_pos)    | // ADC DMA flag
	          (1 << CCONV_pos)  | // ADC conversion complete flag
	          (0 << SCONV_pos)  | // ADC sequence complete flag
	          (0 << CS_pos);      // ADC channel select bits

	EADC = 1;  // Enable ADC interrupt for amplitude measure mode
}
