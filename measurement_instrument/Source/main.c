#include <ADUC841.H>	// special function register definitions
#include "typedef.h"	// variable type definitions
#include "main.h"

// TODO there are WAY too many global variables here, move them into the appropriate functions
static uint16 adc_iir_output;          // IIR filter for ADC samples
static uint16 frequency_iir_output;    // IIR filter for frequency measurements
static uint16 frequency_counter;
static uint16 frequency_value;

// TODO there are WAY too many global variables here, move them into the appropriate functions
static uint16 y_max =0x00; // For amplitude measurement, we need to keep track of the maximum value in the sampling window
static uint16 amplitude_value; // This is the final amplitude value after polynomial interpolation
static uint16 y_min = 0xFFFF; // For amplitude measurement, we also need to keep track of the minimum value in the sampling window
static uint16 amplitude_t1_interrupt_counter;

sfr16 ADCDATA = 0xD9; // TODO why???

void clear_interrupts_and_timers() { // TODO can't we make all these functions 'inline'? Compiler seems not to like the 'inline' keyword
	// Disable all interrupts
	IE = 0; // IE = Interrupt enable. Different bits map to different interrupts so '0 is no interrupts

	// Stop timers that are being used 
	TR0 = 0;   // Stop Timer 0
	TR1 = 0;   // Stop Timer 1
	TR2 = 0;   // Stop Timer 2

	// Clear timer interrupt flags
	TF0 = 0;
	TF1 = 0;
	TF2 = 0;

	// Clear counters to remove leftovers when switching modes
	TH0 = 0; TL0 = 0;
	TH1 = 0; TL1 = 0;
	TH2 = 0; TL2 = 0;
}

/*------------------------------------------------
Interrupt service routine for timer 2 interrupt.
Called by the hardware when the interrupt occurs.
------------------------------------------------*/
void timer2_isr(void) interrupt 5 { // interrupt vector at 002BH
	// TODO split each mode of operation into a separate function here (they can be inlined by compiler)
	// Check what mode we are in:
	if ((SWITCHES&0x03) == 0x01) { // TODO I dislike this direct reading of the switches in the ISR, maybe have a global variable that is set in the main loop when we read the switches and then check that here instead
		// Frequency measurement mode. Increment the register, if it is 25, reset it and record the value on the counter
		frequency_counter++;
		if (frequency_counter >= 25) { // TODO make this == not >=
			frequency_counter = 0;
			// Now record how many pulses T1 has counted in the last 0.1s. Can simply be bit shifted to get the frequency in Hz.
			// Pass frequency through IIR filter to smooth it out
			frequency_value = (TH1 << 8) | TL1;
			update_display_via_iir(frequency_value);
			// Reset timer 1 for the next measurement
			TH1 = 0;
			TL1 = 0;
		}
	}
	else if ((SWITCHES&0x03) == 0x02) {
			//TODO check if this could be smaller
			if (amplitude_t1_interrupt_counter >= 17){
			amplitude_value = y_max - y_min; // Get the amplitude value for the sampling window

			// Reset the max and min for the next sampling window
			y_max = 0x00;
			y_min = 0xFFFF;
			amplitude_t1_interrupt_counter = 0;
			update_display_via_iir(amplitude_value);
			}
			else{
				amplitude_t1_interrupt_counter ++;
			}
	}
	TF2 = 0; // Clear the interrupt
}  // end timer2 interrupt service routine

void adc_isr(void) interrupt 6 {
	uint16 sample_value = (ADCDATAH <<8 | ADCDATAL) & 0x0FFF; //TODO is this correct?
	if ((SWITCHES&0x03) == 0x00) { // TODO I don't like that we are reading the switches directly in the ISR
		// DC measurement mode. Take the sample and update the IIR filter output
		// IIR Filter
		sample_value = sample_value & 0x0FFF;
		update_display_via_iir(sample_value);

	} else if ((SWITCHES&0x03) == 0x02) {
		// Record the highest and lowest value
		if (sample_value > y_max){
			y_max = sample_value;
		}

		if (sample_value < y_min){
			y_min = sample_value;
		}
	}
}

void setup_dc_measure(void) {
	// TODO we're using timer 2 here?
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

void setup_frequency_measure(void) {
	// Frequency measurement mode.
	// Uses timer 2 to generate an ISR every 0.1 seconds
	// and timer 0 to count positive edges on the input signal
	// every 0.1 s, the value in timer 0 is read and displayed (via the IIR filter),
	// then timer 0 is reset

	// Configure timer 2 to generate an interrupt every 0.1s (needs a reload value of 10240, and a register counting to 25)
	T2CON = (0 << TF2_pos)     | // Overflow flag
	        (0 << EXF2_pos)    | // External flag
	        (0 << RCLK_pos)    | // receive flag enable
	        (0 << TCLK_pos)    | // transmit clock enable
	        (0 << EXEN2_pos)   | // external enable flag
	        (1 << TR2_pos)     | // start/stop bit
	        (0 << CNT2_pos)    | // Timer/counter mode
	        (0 << CAP2_pos);     // Capture/reload mode

	// Enable Timer 2 interrupt
	//IE = // TODO write entire IE register appropriately and do the same for amplitude measure mode
	ET2 = 1;
	ET1 = 0;
	ET0 = 0;
	EA = 1;    // Enable global interrupts
	EADC = 0;  // Disable ADC interrupt

	// Set reload value to 10240
	RCAP2L = 0x00;
	RCAP2H = 0x28;

	// Configure timer 0 to count the input frequency. Use in mode 1.
	//TMOD = (0 << GATE_pos) | // Timer 0 gate control
	//       (1 << CT_pos)   | // Timer/counter mode
	//       (0 << M1_pos)   | // Mode bit 1
	//       (1 << M0_pos);    // Mode bit 0
	TMOD = 0x50; //0b00000101; // look at diagram on page 70 for mode 1. Gate is LOW so that we are always counting, and timer is connected to P3.4

	TCON =	(0 << TF1_pos) | // Timer 1 overflow flag off
					(1 << TR1_pos) | // Timer 1 should be running
					(0 << TF0_pos) | // Timer 0 overflow flag
					(0 << TR0_pos) | // Timer 0 run control
					(0 << IE1_pos) | // External interrupt 1 flag
					(0 << IT1_pos) | // External interrupt 1 type
					(0 << IE0_pos) | // External interrupt 0 flag
					(0 << IT0_pos);   // External interrupt 0 type
				 
}

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

	// Use Timer 2 to measure sampling window, pulse every 0.1s
	T2CON = (0 << TF2_pos)     | // Overflow flag
	        (0 << EXF2_pos)    | // External flag
	        (0 << RCLK_pos)    | // receive flag enable
	        (0 << TCLK_pos)    | // transmit clock enable
	        (0 << EXEN2_pos)   | // external enable flag
	        (1 << TR2_pos)     | // start/stop bit
	        (0 << CNT2_pos)    | // Timer/counter mode
	        (0 << CAP2_pos);     // Capture/reload mode
	
	// Set reload value to 0, use a register to count 17 pulses to get a sampling window of 0.1s
	RCAP2L = 0x00;
	RCAP2H = 0x00;

	EA = 1;			// Enable global interrupts // TODO use a single write to IE
	EADC = 1;		// Enable ADC interrupt
	ET2 = 1;		// Enable Timer 2 interrupt
}

void main(void) {
	uint8 prev_switch_value;

	initialDisplaySetup(); // set up the SPI display, including a display test long enough for humans to see all LEDs light up

	//i = 0;
	//while(1) {
	//	i++;
	//	update_display_via_iir(i);
	//}

	// Configure switches for user input and begin reading their values
	SWITCHES = 0xFF;  // Make switch pins inputs
	T1 = 1; // put timer 1 counter input in input mode  TODO why is this in main??
	prev_switch_value = 0x8;

	while(1) {
		// Read switches and enter appropriate mode
		uint8 switch_value = SWITCHES & 0x03;
		if (prev_switch_value != switch_value) {
			clear_interrupts_and_timers(); // If we are switching mode, disable interrupts and clear timers before switching modes to avoid any issues with leftover state from the previous mode
			reset_iir(); // Reset the IIR filter for the display when we switch modes (this will set a flag that causes it to overwrite the IIR value on the next update, then return to normal IIR operation)

			if (switch_value == 0x00) { // TODO enumerate these hardcoded values
				// DC measurement mode
				spiWrite(MAX7219_DIGIT8_ADDR,				1); // TODO replace these SPI writes with using the LEDs on port 0
				setup_dc_measure();
			} else if (switch_value == 0x01) {
				// Frequency measurement mode
				spiWrite(MAX7219_DIGIT8_ADDR,				2);
				setup_frequency_measure();
			} else if (switch_value == 0x02) {
				// Amplitude measurement mode
				spiWrite(MAX7219_DIGIT8_ADDR,				3);
				setup_amplitude_measure();
			} else {
				spiWrite(MAX7219_DIGIT8_ADDR,				0);
				//setup_dc_measure();
			}
		}
		prev_switch_value = switch_value;
	}
}  // end main
