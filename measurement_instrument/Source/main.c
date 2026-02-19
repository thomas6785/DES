#include <ADUC841.H>	// special function register definitions
#include "typedef.h"	// variable type definitions

#define SWITCHES  	P2		// switches are connected to Port 2

#define TF2_pos 			(7)
#define EXF2_pos 			(6)
#define RCLK_pos 			(5)
#define TCLK_pos			(4)
#define EXEN2_pos 			(3)
#define TR2_pos				(2)
#define CNT2_pos 			(1)
#define CAP2_pos 			(0)

#define MD1_pos 			(7)
#define EXT_REF_pos 		(6)
#define CK1_pos 			(5)
#define CK0_pos				(4)
#define AQ_pos 				(2)
#define T2C_pos 			(1)
#define EXC_pos 			(0)

#define ADCI_pos 			(7)
#define DMA_pos 			(6)
#define CCONV_pos 			(5)
#define SCONV_pos			(4)
#define CS_pos 				(0)

// TCON definitions
#define TF1_pos 			(7)
#define TR1_pos 			(6)
#define TF0_pos 			(5)
#define TR0_pos 			(4)
#define IE1_pos 			(3)
#define IT1_pos 			(2)
#define IE0_pos 			(1)
#define IT0_pos 			(0)

// TMOD definitions for timer 0
#define GATE_pos 			(3)
#define CT_pos 				(2)
#define M1_pos 				(1)
#define M0_pos 				(0)

static uint16 adc_iir_output;          // IIR filter for ADC samples
static uint16 frequency_iir_output;    // IIR filter for frequency measurements
static uint16 frequency_counter;
static uint16 frequency_value;
/*------------------------------------------------
Interrupt service routine for timer 2 interrupt.
Called by the hardware when the interrupt occurs.
------------------------------------------------*/
void timer2(void) interrupt 5   // interrupt vector at 002BH
{
	// Check what mode we are in:
	if ((SWITCHES&0x03) == 0x01) {
		// Frequency measurement mode. Increment the register, if it is 25, reset it and record the value on the counter
		frequency_counter++;
		if (frequency_counter >= 25) {
			frequency_counter = 0;
			// Now record how many pulses T0 has counted in the last 0.125s. Can simply be bit shifted to get the frequency in Hz.
			// Pass frequency through IIR filter to smooth it out
			frequency_value = TL0 + (TH0 << 8);
			frequency_value = ((7*frequency_value) >> 3) + (frequency_iir_output >> 3);
		}
	}
}  // end timer2 interrupt service routine


void adc_isr(void) interrupt 6
{
	// Needs to take current sample, update the average
	// Be careful this operation is atomic

	// IIR Filter
	uint16 sample_value = ADCDATAL;
	sample_value = sample_value & 0x0FFF;
	adc_iir_output = ((7*sample_value) >> 3) + (adc_iir_output >> 3);
}



void dc_measure(void) {
	T2CON = (0 << TF2_pos)     | // Overflow flag
	        (0 << EXF2_pos)    | // External flag
	        (0 << RCLK_pos)    | // receive flag enable
	        (0 << TCLK_pos)    | // transmit clock enable
	        (0 << EXEN2_pos)   | // external enable flag
	        (1 << TR2_pos)     | // start/stop bit
	        (0 << CNT2_pos)    | // Timer/counter mode
	        (0 << CAP2_pos);     // Capture/reload mode

	// Configure ADC for DC
	ADCCON1 = (1 << MD1_pos)    | // Operating mode of ADC
	          (0 << EXT_REF_pos)| // External reference
	          (0 << CK1_pos)    | // ADC clock divide bits
	          (0 << CK0_pos)    | // ADC clock divide bits
	          (3 << AQ_pos)     | // number of ADC clock cycles
	          (1 << T2C_pos)    | // Which timer to use
	          (0 << EXC_pos);     // external trigger

	// Enable ADC interrupt
	ADCCON2 = (1 << ADCI_pos)   | // ADC interrupt flag
	          (0 << DMA_pos)    | // ADC DMA flag
	          (0 << CCONV_pos)  | // ADC conversion complete flag
	          (0 << SCONV_pos)  | // ADC sequence complete flag
	          (0 << CS_pos);      // ADC channel select bits
	EA = 1;    // Enable global interrupts
	EADC = 1;  // Enable ADC interrupt
}


void frequency_measure(void) {
	// Configure timer 2 to generate a pulse every 0.1s (needs a reload value of 10240, and a register counting to 25)
	T2CON = (0 << TF2_pos)     | // Overflow flag
	        (0 << EXF2_pos)    | // External flag
	        (0 << RCLK_pos)    | // receive flag enable
	        (0 << TCLK_pos)    | // transmit clock enable
	        (0 << EXEN2_pos)   | // external enable flag
	        (1 << TR2_pos)     | // start/stop bit
	        (0 << CNT2_pos)    | // Timer/counter mode
	        (0 << CAP2_pos);     // Capture/reload mode

	// Enable Timer 2 interrupt
	ET2 = 1;
	EA = 1;    // Enable global interrupts

	// Set reload value to 10240
	RCAP2L = 0x00;
	RCAP2H = 0x28;

	// Configure timer 0 to count the input frequency. Use in mode 1.
	TMOD = (0 << GATE_pos) | // Timer 0 gate control
	       (1 << CT_pos)   | // Timer/counter mode
	       (0 << M1_pos)   | // Mode bit 1
	       (1 << M0_pos);    // Mode bit 0

	TCON = (0 << TF0_pos) | // Timer 0 overflow flag
	       (1 << TR0_pos) | // Timer 0 run control
	       (0 << IE1_pos) | // External interrupt 1 flag
	       (0 << IT1_pos) | // External interrupt 1 type
	       (0 << IE0_pos) | // External interrupt 0 flag
	       (0 << IT0_pos);   // External interrupt 0 type


}


void main(void) {
	uint8 prev_switch_value;
	SWITCHES = 0xFF;  // Make switch pins inputs
	prev_switch_value = 0x8;

	while(1) {
		// Read switches and enter appropriate mode
		uint8 switch_value = SWITCHES & 0x03;		
		if (prev_switch_value != switch_value){
			
			if (switch_value == 0x00) {
				// DC measurement mode
				dc_measure();
			} else if (switch_value == 0x01) {
				// Frequency measurement mode
				frequency_measure();
			} else if (switch_value == 0x02) {
				// IIR filter test mode
				// TODO: Implement IIR filter test mode
			} else {
				// Default to DC measurement mode (switch_value == 0x03)
				dc_measure();
			}
		}
		prev_switch_value = switch_value;
		
		
	}
}  // end main
	