#include <ADUC841.H>	// special function register definitions
#include "typedef.h"	// variable type definitions

#define TF2_pos 	(7)
#define EXF2_pos 	(6)
#define RCLK_pos 	(5)
#define TCLK_pos	(4)
#define EXEN2_pos (3)
#define TR2_pos		(2)
#define CNT2_pos 	(1)
#define CAP2_pos 	(0)

#define MD1_pos 		(7)
#define EXT_REF_pos (6)
#define CK1_pos 		(5)
#define CK0_pos			(4)
#define AQ_pos 		(2)
#define T2C_pos 		(1)
#define EXC_pos 		(0)

#define ADCI_pos 			(7)
#define DMA_pos 			(6)
#define CCONV_pos 		(5)
#define SCONV_pos			(4)
#define CS_pos 				(0)

static uint16 iir_output;
/*------------------------------------------------
Interrupt service routine for timer 2 interrupt.
Called by the hardware when the interrupt occurs.
------------------------------------------------*/
void timer2 (void) interrupt 5   // interrupt vector at 002BH
{
	
}	// end timer2 interrupt service routine


void adc_isr (void) interrupt 6
{
	// Needs to take current sample, update the average
	// Be carefult this operation is atomic
	
	// IIR Filter
	uint16 sample_value = ADCDATAL;
	sample_value = sample_value & 0x0FFF;
	iir_output = ((7*sample_value) >> 3) + (iir_output >>3);
}

void main (void) {
	
	// Enable timer 2
	T2CON = (0 << TF2_pos)		| //Overflow flag
					(0 << EXF2_pos)	| //External flag
					(0 << RCLK_pos)	|	// receive flag enable
					(0 << TCLK_pos)	|	// transmist clock enable
					(0 << EXEN2_pos)	| // external enable flag
					(1 << TR2_pos)		| // start/stop bit
					(0 << CNT2_pos)	| // Timer/counter mode
					(0 << CAP2_pos); 	// Capture/reload mode
	
	// Configure ADC for DC
	ADCCON1 = (1 << MD1_pos)		| //Operating mode of ADC
						(0 << EXT_REF_pos)	| //External reference
						(0 << CK1_pos)	|	// ADC clock divide bits
						(0 << CK0_pos)	|	// ADC clock divide bits
						(3 << AQ_pos)	| // number of ADC clock cycles
						(1 << T2C_pos)	| // Which timer to use
						(0 << EXC_pos); 	// external trigger
	
	ADCCON2 = (0 << ADCI_pos)		| 
						(0 << DMA_pos)	| 
						(0 << CCONV_pos)	|	
						(0 << SCONV_pos)	|	
						(0 << CS_pos); 
			// Enable the ADC interrupt	
	EA = 1;
	EADC = 1;
	
	
		while(1)
		{
			// Read switches
			
			// Update display
		}
	
}