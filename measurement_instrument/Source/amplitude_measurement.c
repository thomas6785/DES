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

static uint16 y_store;
static uint16 y_max;
static uint16 y_max_minus_1;
static uint16 y_max_plus_1;
static uint16 y_min;
static uint16 y_min_minus_1;
static uint16 y_min_plus_1;
static uint8 prev_was_highest;
static uint8 prev_was_lowest;

void amplitude_handle_sample(uint32 sample_value) {

	// Update the peak detection algorithm
	if (sample_value > y_max) {
		y_max = sample_value;
		prev_was_highest = 1;
		y_max_minus_1 = y_store;
	}
	else if (prev_was_highest) {
		y_max_plus_1 = sample_value;
		prev_was_highest = 0;
	} 

	if (sample_value < y_min) {
		y_min = sample_value;
		prev_was_lowest = 1;
		y_min_minus_1 = y_store;
	}
	else if (prev_was_lowest) {
		y_min_plus_1 = sample_value;
		prev_was_lowest = 0;
	} 

	y_store = sample_value;
	// Needs to take current sample, update the average
	// Be careful this operation is atomic
}

uint32 get_amplitude_measurement(void) {
	//TODO check if this could be smaller
	int32 amplitude_max;
	int32 amplitude_min;
	// Amplitude measurement mode. Reset the peak detection algorithm and calculate the actual maximum value using polynomial interpolation
	// TODO check denom not zero?
	amplitude_max = (int32)y_max - ((int32)y_max_minus_1 - (int32)y_max_plus_1) * (((int32)y_max_minus_1 - (int32)y_max_plus_1) / ((int32)(y_max_minus_1 - 2*(int32)y_max + (int32)y_max_plus_1)) >> 2);
	amplitude_min = (int32)y_min - ((int32)y_min_minus_1 - (int32)y_min_plus_1) * (((int32)y_min_minus_1 - (int32)y_min_plus_1) / ((int32)(y_min_minus_1 - 2*(int32)y_min + (int32)y_min_plus_1)) >> 2);

	// Reset peak detection
	y_max = 0;
	y_max_minus_1 = 0;
	y_max_plus_1 = 0;
	y_store = 0;
	y_min = 0xFFFF;
	y_min_minus_1 = 0;
	y_min_plus_1 = 0;
	prev_was_highest = 0;
	prev_was_lowest = 0;
	//return (amplitude_max - amplitude_min):
	return (y_max - y_min);
}
