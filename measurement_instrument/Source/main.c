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

// TCON definitions
#define TF1_pos 			(7)
#define TR1_pos 			(6)
#define TF0_pos 			(5)
#define TR0_pos 			(4)
#define IE1_pos 			(3)
#define IT1_pos 			(2)
#define IE0_pos 			(1)
#define IT0_pos 			(0)

#define SWITCHES  	P2	
static uint16 iir_output;
static unit16 max_sample = 0;     //store the highest value in block
static unit16 min_sample = 0x0FFF;  // store the lowest value in block
static unit16 sample_count = 0;   // count samples per block
static unit8 block_complete = 0;    // flag to signal main loop
/*------------------------------------------------
Interrupt service routine for timer 2 interrupt.
Called by the hardware when the interrupt occurs.
------------------------------------------------*/

void display_vpp_mV(unit32 vpp_mV)
{
// SPI 8 digit display 
	(void)vpp_mV;
	
}
void timer2 (void) interrupt 5   // interrupt vector at 002BH
{
	
}	// end timer2 interrupt service routine


void adc_isr (void) interrupt 6
{
 unit16 current_sample = ADCDATAL & 0x0FFF; // capture the 12-bit sample [2, 3]
	
	//track the maximum and minimum  for the peak to peak
	if ( current_sample > max_sample) max_sample = current_sample;
	if (current_sample < min_sample) min_sample = current_sample;
	
	sample_count++;
	
	// block size 
	if (sample_count >= 512) { //should still calculate 
	 block_complete = 1;
		

	}
}

void main (void) {
	
	unit16 min_c, max_c;
	unit16 vpp_counts;
	unit32 vpp_mV;
	
	
	// Set reload value 
	RCAP2L = 0x00;
	RCAP2H = 0x28;
	
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
						(2 << CS_pos); 
						

						
			// Enable the ADC interrupt	
	EA = 1;
	EADC = 1;
	

	
		while(1)
		{
		if (block_ready)
		{
			EA = 0;
			min_c = min_sample;
			max_c = max_sample;
			
			// reset then again for next block
			min_sample = 0x0FFF;
			max_sample = 0x0000;
			sample_count = 0;
			block_ready -0;
			EA = 1;
			
			//need to convert to input as per  Vpp(mV)
		
		}
	
}