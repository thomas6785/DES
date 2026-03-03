#include <ADUC841.H>	// special function register definitions
#include "typedef.h"	// variable type definitions

#define SWITCHES  	P2		// switches are connected to Port 2
#define LOAD 				RXD

#define TF2_pos 			(7)
#define EXF2_pos 			(6)
#define RCLK_pos 			(5)
#define TCLK_pos			(4)
#define EXEN2_pos 		(3)
#define TR2_pos				(2)
#define CNT2_pos 			(1)
#define CAP2_pos 			(0)

#define MD1_pos 			(7)
#define EXT_REF_pos 	(6)
#define CK1_pos 			(5)
#define CK0_pos				(4)
#define AQ_pos 				(2)
#define T2C_pos 			(1)
#define EXC_pos 			(0)

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

// TMOD definitions for timer 0
#define GATE_pos 			(3)
#define CT_pos 				(2)
#define M1_pos 				(1)
#define M0_pos 				(0)

// SPI Con
#define ISPI_pos				(7) // Interrupt bit
#define WCOL_pos				(6) // Write collision bit
#define SPE_pos					(5) // SPI interface enable
#define SPIM_pos				(4) // set for master, clear for slave mode
#define CPOL_pos				(3) // set for clock to idle high, clear for idle low
#define CPHA_pos				(2) // clock polarity. Set for leading edge to transmti, clear for trailing edge to transmit
#define SPR_pos					(0) // select SCLCK rate - see data sheet

// Interrupt enable register bit positions
#define EA_pos					(7) // Global interrupt enable bit
#define EADC_pos				(6) // ADC interrupt enable bit
#define ET2_pos					(5) // Timer 2 interrupt enable bit
#define ES_pos					(4) // Serial interrupt enable bit
#define ET1_pos					(3) // Timer 1 interrupt enable bit
#define EX1_pos					(2) // External interrupt 1 enable bit
#define ET0_pos					(1) // Timer 0 interrupt enable bit
#define EX0_pos					(0) // External interrupt 0 enable bit

// Register addresses on the MAX7219 display
#define MAX7219_DIGIT1_ADDR				(1)
#define MAX7219_DIGIT2_ADDR				(2)
#define MAX7219_DIGIT3_ADDR				(3)
#define MAX7219_DIGIT4_ADDR				(4)
#define MAX7219_DIGIT5_ADDR				(5)
#define MAX7219_DIGIT6_ADDR				(6)
#define MAX7219_DIGIT7_ADDR				(7)
#define MAX7219_DIGIT8_ADDR				(8)
#define MAX7219_DECODE_MODE_ADDR	(9)
#define MAX7219_INTENSITY_ADDR		(10)
#define MAX7219_SCAN_LIMIT_ADDR		(11)
#define MAX7219_SHUTDOWN_ADDR			(12)
#define MAX7219_DISPLAY_TEST_ADDR	(15)




void update_display_wrap(uint16 value);


// TODO there are WAY too many global variables here, move them into the appropriate functions
static uint16 adc_iir_output;          // IIR filter for ADC samples
static uint16 frequency_iir_output;    // IIR filter for frequency measurements
static uint16 frequency_counter;
static uint16 frequency_value;

static uint16 y_max; // For amplitude measurement, we need to keep track of the maximum value in the sampling window
static uint16 y_max_minus_1; // We need to keep track of the sample before the maximum value to implement the peak detection algorithm for amplitude measurement

static uint16 y_max_plus_1; // We need to keep track of the sample after the maximum value to implement the peak detection algorithm for amplitude measurement

static uint16 y_store; // We need to store the current sample 
static uint16 amplitude_value; // This is the final amplitude value after polynomial interpolation

static uint8 prev_was_highest; // This is a flag to indicate whether the previous sample was the highest value in the sampling window, used for peak detection in amplitude measurement mode
static uint16 y_min = 0xFFFF; // For amplitude measurement, we also need to keep track of the minimum value in the sampling window
static uint16 y_min_minus_1;
static uint16 y_min_plus_1;
static uint8 prev_was_lowest; 
static uint16 amplitude_t1_interrupt_counter;

sfr16 ADCDATA = 0xD9;


static void disable_all_interrupts_and_timers(void)
	{
	EA = 0;    // disable global interrupts while configuring 

	// disable interrupts being used 
	EADC = 0;  // ADC interrupt disable
	ET0  = 0;  // Timer 0 interrupt disable
	ET2  = 0;  // Timer 2 interrupt disable

	// stop timers that are being used 
	TR0 = 0;   // Stop Timer 0
	TR2 = 0;   // Stop Timer 2

	// Clear timer interrupt flags
	TF0 = 0;
	TF2 = 0;

	//clear counter registers to remove leftovers across modes
	TH0 = 0; TL0 = 0;
	TH2 = 0; TL2 = 0;


}
	

/*------------------------------------------------
Interrupt service routine for timer 2 interrupt.
Called by the hardware when the interrupt occurs.
------------------------------------------------*/
void timer2(void) interrupt 5 { // interrupt vector at 002BH
	// Check what mode we are in:
	if ((SWITCHES&0x03) == 0x01) {
		// Frequency measurement mode. Increment the register, if it is 25, reset it and record the value on the counter
		frequency_counter++;
		if (frequency_counter >= 25) { // TODO make this == not >=
			frequency_counter = 0;
			// Now record how many pulses T0 has counted in the last 0.1s. Can simply be bit shifted to get the frequency in Hz.
			// Pass frequency through IIR filter to smooth it out
			frequency_value = (TH0 << 8) | TL0;
			update_display_wrap(frequency_value);
		}
	}
	TF2 = 0; // Clear the interrupt
}  // end timer2 interrupt service routine

void timer0(void) interrupt 1 {
	// This is the sampling window timer for amplitude measurement mode. We need to reset the peak detection algorithm at the end of each sampling window.
	// and use the polynomial interpolation algorithm to calculate the actual maximum value based on y_max, y_max_minus_1, and y_max_plus_1
	if ((SWITCHES&0x03) == 0x02) {
		if (amplitude_t1_interrupt_counter >= 17){
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
			amplitude_t1_interrupt_counter = 0;
			amplitude_value = amplitude_max - amplitude_min;
			update_display_wrap(amplitude_value);
		} else{
			amplitude_t1_interrupt_counter++;
		}
	}
	TF0 = 0; // Clear the interrupt
}

void adc_isr(void) interrupt 6 {
	if ((SWITCHES&0x03) == 0x00) {
		// DC measurement mode. Take the sample and update the IIR filter output
		// IIR Filter
		uint16 sample_value = ADCDATA & 0x0FFF; //TODO is this correct?
		sample_value = sample_value & 0x0FFF;
		update_display_wrap(sample_value); // TODO find a systematic way to reset IIR filter after mode switch

	} else if ((SWITCHES&0x03) == 0x02) {
		// Amplitude measurement mode. 
		uint16 prev_sample_value = y_store;
		uint16 sample_value = ADCDATA & 0x0FFF;

		// Update the peak detection algorithm
		if (sample_value > y_max) {
			y_max = sample_value;
			prev_was_highest = 1;
			y_max_minus_1 = prev_sample_value;
		}
		else if (prev_was_highest) {
			y_max_plus_1 = sample_value;
			prev_was_highest = 0;
		} 

		if (sample_value < y_min) {
			y_min = sample_value;
			prev_was_lowest = 1;
			y_min_minus_1 = prev_sample_value;
		}
		else if (prev_was_lowest) {
			y_min_plus_1 = sample_value;
			prev_was_lowest = 0;
		} 

		y_store = sample_value;
	}
	// Needs to take current sample, update the average
	// Be careful this operation is atomic
}

void setup_dc_measure(void) {
	disable_all_interrupts_and_timers();
	
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
						
	// DC mode: enable only the ADC interrupt 	
  
 
	EA = 1;    // Enable global interrupts
	EADC = 1;  // Enable ADC interrupt
}

void setup_frequency_measure(void) {
	// Frequency measurement mode.
	// Uses timer 2 to generate an ISR every 0.1 seconds
	// and timer 0 to count positive edges on the input signal
	// every 0.1 s, the value in timer 0 is read and displayed (via the IIR filter),
	// then timer 0 is reset
	
	
	disable_all_interrupts_and_timers();

	// Configure timer 2 to generate an interrupt every 0.1s (needs a reload value of 10240, and a register counting to 25)
	T2CON = (0 << TF2_pos)     | // Overflow flag
	        (0 << EXF2_pos)    | // External flag
	        (0 << RCLK_pos)    | // receive flag enable
	        (0 << TCLK_pos)    | // transmit clock enable
	        (0 << EXEN2_pos)   | // external enable flag
	        (1 << TR2_pos)     | // start/stop bit
	        (0 << CNT2_pos)    | // Timer/counter mode
	        (0 << CAP2_pos);     // Capture/reload mode

	
	// Set reload value to 10240
	RCAP2L = 0x00;
	RCAP2H = 0x28;

	// Configure timer 0 to count the input frequency. Use in mode 1.
	//TMOD = (0 << GATE_pos) | // Timer 0 gate control
	//       (1 << CT_pos)   | // Timer/counter mode
	//       (0 << M1_pos)   | // Mode bit 1
	//       (1 << M0_pos);    // Mode bit 0
	TMOD = 0x05; //0b00000101; // look at diagram on page 70 for mode 1. Gate is LOW so that we are always counting, and timer is connected to P3.4

	TCON = (0 << TF0_pos) | // Timer 0 overflow flag
	       (1 << TR0_pos) | // Timer 0 run control
	       (0 << IE1_pos) | // External interrupt 1 flag
	       (0 << IT1_pos) | // External interrupt 1 type
	       (0 << IE0_pos) | // External interrupt 0 flag
	       (0 << IT0_pos);   // External interrupt 0 type
				 
				 // Enable Timer 2 interrupt
	//IE = // TODO write entire IE register appropriately and do the same for amplitude measure mode
	ET2 = 1;
	EA = 1;    // Enable global interrupts
	

}

void setup_amplitude_measure(void) {
	
	disable_all_interrupts_and_timers();

	// Configure time 2 to generate a pulse with frequency 5.5296MHz. Need a reload value of 65534, and to be in timer mode
	T2CON = (0 << TF2_pos)     | // Overflow flag
	        (0 << EXF2_pos)    | // External flag
	        (0 << RCLK_pos)    | // receive flag enable
	        (0 << TCLK_pos)    | // transmit clock enable
	        (0 << EXEN2_pos)   | // external enable flag
	        (1 << TR2_pos)     | // start/stop bit
	        (0 << CNT2_pos)    | // Timer/counter mode
	        (0 << CAP2_pos);     // Capture/reload mode

	// Set reload value to 65534
	RCAP2L = 0xFF;
	RCAP2H = 0xFE;

	// Configure ADC to measure with frequency 150kHz
	ADCCON1 = (1 << MD1_pos)    | // Operating mode of ADC
	          (0 << EXT_REF_pos)| // External reference
	          (0 << CK1_pos)    | // ADC clock divide bits
	          (0 << CK0_pos)    | // ADC clock divide bits
	          (0 << AQ_pos)     | // number of ADC clock cycles
	          (1 << T2C_pos)    | // Which timer to use
	          (0 << EXC_pos);     // external trigger

	ADCCON2 = (1 << ADCI_pos)   | // ADC interrupt flag
	          (0 << DMA_pos)    | // ADC DMA flag
	          (0 << CCONV_pos)  | // ADC conversion complete flag
	          (0 << SCONV_pos)  | // ADC sequence complete flag
	          (0 << CS_pos);      // ADC channel select bits

	// We need to configure the sampling window using Timer 0. Set it to generate a pulse every 0.1s (using a register).
	TMOD = (0 << GATE_pos) | // Timer 0 gate control
	       (0 << CT_pos)   | // Timer/counter mode
	       (0 << M1_pos)   | // Mode bit 1
	       (0 << M0_pos);    // Mode bit 0
	
	TCON = (0 << TF1_pos) | // Timer 1 overflow flag
	       (0 << TR1_pos) | // Timer 1 run control
	       (0 << TF0_pos) | // Timer 0 overflow flag
	       (0 << IT1_pos) | // External interrupt 1 type
	       (0 << IE0_pos) | // External interrupt 0 flag
	       (0 << IT0_pos);   // External interrupt 0 type
	
	EADC =1; // Enable ADC interrupt
	EA = 1;    // Enable global interrupts
	ET0 = 1;   // Enable Timer 0 interrupt
	
}

void spiWrite(uint8 address, uint8 data_value) {
	int i;

	LOAD = 0;						// load goes low at start

	// Write the address byte
	SPIDAT = address;		// send address byte (writing to SPIdat will trigger the SPI transmission too)
	while(~ISPI);				// wait until ISPI is 1 indicating the SPI write is done
	ISPI = 0;						// reset ISPI

	// Need a small delay between SPI transactions
	for(i=0; i <=10000; i++){} // TODO shorten delay - not clear if it is necessary or not?

	// Write the data byte
	SPIDAT = data_value;	// send data byte
	while(~ISPI);					// wait until ISPI is 1 indicating the SPI write is done
	ISPI = 0;							// reset ISPI

	LOAD = 1;							// load goes high at end to tell the peripheral to update

	for(i=0; i <=10000; i++); //Delay
	// TODO remove this delay - not clear if it is necessary or not?
}

void display_value(uint16 value) {// TODO should be int not uint
	// Take in a 16-bit binary value and convert to decimal, then
	// display on the 7-segment display
	int i;
	uint8 bcd[3]; // Array to hold the BCD digits. bcd[0] is the ones and tens, bcd[1] is the hundreds and thousands, etc.
	char sign; // Assume input is always positive for now, so sign bit is 0 TODO use something smaller than a char
	// TODO maybe just use a 32-bit word instead of 3 8-bit words since the compiler will be smarter about bitshifting then
	// If the value is negative, flip the sign bit and make the value positive for the BCD conversion
	//if (value < 0) {
	//	sign = 1;
	//	value = -value;
	//} else {
	//	sign = 0;
	//}
	sign = 0;

	bcd[0] = 0; // Ones and tens
	bcd[1] = 0; // hundreds and thousands
	bcd[2] = 0; // tens of thousands TODO merge sign bit into MSB of this byte

	for (i = 0; i<16; i++) {
		// if any BCD digit has more than 5, increment it by 3
		if (i != 0) { // if statement so we can skip this the first time round
			if ((bcd[0] & 0x0F) > 0x04) { bcd[0] += 0x03; }
			if ((bcd[0] & 0xF0) > 0x40) { bcd[0] += 0x30; }
			if ((bcd[1] & 0x0F) > 0x04) { bcd[1] += 0x03; }
			if ((bcd[1] & 0xF0) > 0x40) { bcd[1] += 0x30; }
			if ((bcd[2] & 0x0F) > 0x04) { bcd[2] += 0x03; }
		}

		// shift a bit into the BCD array
		bcd[2] = bcd[2] << 1;											// Shift bcd[2] left by 1
		bcd[2] = bcd[2] | (0x01 & (bcd[1] >> 7));	// Move the MSB of bcd[1] to the LSB of bcd[2]
		bcd[1] = bcd[1] << 1;											// Shift bcd[1] left by 1
		bcd[1] = bcd[1] | (0x01 & (bcd[0] >> 7));	// Move the MSB of bcd[0] to the LSB of bcd[1]
		bcd[0] = bcd[0] << 1;											// Shift bcd[0] left by 1
		bcd[0] = bcd[0] | (0x01 & (value >> 15));	// Shift the MSB of the input binary value into the LSB of bcd[0]
		value = value << 1;								// Shift input value left by 1 to get the next bit into the MSB
	}
	
	// Write the BCD to the display using SPI
	spiWrite(MAX7219_DIGIT1_ADDR,		bcd[0] & 0x0F);	// Ones
	spiWrite(MAX7219_DIGIT2_ADDR,		(bcd[0] & 0xF0) >> 4);	// Tens
	spiWrite(MAX7219_DIGIT3_ADDR,		bcd[1] & 0x0F);	// Hundreds
	spiWrite(MAX7219_DIGIT4_ADDR,		(bcd[1] & 0xF0) >> 4);	// Thousands
	spiWrite(MAX7219_DIGIT5_ADDR,		bcd[2] & 0x0F);	// Ten thousands
	// TODO also write a sign bit to the display
	// TODO and maybe write to the more significant digits
}

void update_display_wrap(uint16 value) {
	// Wrapper function which updates an IIR filter and then calls display_value to update the display.
	static uint16 display_iir_output = 0; // IIR filter for display value. Initialise to zero. STATIC VARIABLE so it will persist across function calls

	// Update IIR filter
	display_iir_output = ((7*value) >> 3) + (display_iir_output >> 3);

	display_value(display_iir_output);
}

void main(void) {
	int i;
	uint8 prev_switch_value;
	
	SPICON =	(0 << ISPI_pos)	|
	 					(0 << WCOL_pos)	|
	 					(1 << SPE_pos)	|
	 					(1 << SPIM_pos)	|
	 					(0 << CPOL_pos)	|
	 					(0 << CPHA_pos)	|
	 					(3 << SPR_pos);
	
	//spiWrite(MAX7219_DISPLAY_TEST_ADDR,	1);		// Run display test
	//for(i=0; i <=1000000; i++); //Delay for the display test to be visible to humans
	//spiWrite(MAX7219_DISPLAY_TEST_ADDR,	0);		// Disable display test

	spiWrite(MAX7219_SHUTDOWN_ADDR,			1);			// Switch on the display
	spiWrite(MAX7219_DIGIT1_ADDR,				0);			// Some digits to display
	spiWrite(MAX7219_DIGIT2_ADDR,				0);			// Some digits to display
	spiWrite(MAX7219_DIGIT3_ADDR,				0);			// Some digits to display
	spiWrite(MAX7219_DIGIT4_ADDR,				0);			// Some digits to display
	spiWrite(MAX7219_DIGIT5_ADDR,				0);			// Some digits to display
	spiWrite(MAX7219_DIGIT6_ADDR,				0);			// Some digits to display
	spiWrite(MAX7219_DIGIT7_ADDR,				0);			// Some digits to display
	spiWrite(MAX7219_DIGIT8_ADDR,				0);			// Some digits to display
	spiWrite(MAX7219_DECODE_MODE_ADDR,	0xFF);	// Set to '1' to use a LUT to display digits using the 7 segment display
	spiWrite(MAX7219_INTENSITY_ADDR,		15);		// Set to 15 for max brightness (for now TODO)
	spiWrite(MAX7219_SCAN_LIMIT_ADDR,		7);			// Set to 7 for 8 digits
	
	i = 0;
	//while(1) {
	//	i++;
	//	update_display_wrap(i); // Display a test value to make sure everything is working
	//}

	// Configure switches for user input and begin reading their values
	SWITCHES = 0xFF;  // Make switch pins inputs
	T0 = 1; // put this in input mode TODO wtf
	prev_switch_value = 0x8;

	while(1) {
		// Read switches and enter appropriate mode
		uint8 switch_value = SWITCHES & 0x03;		
		if (prev_switch_value != switch_value) {
			if (switch_value == 0x00) { // TODO enumerate these hardcoded values
				// DC measurement mode
				spiWrite(MAX7219_DIGIT8_ADDR,				1);
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
				// Default to DC measurement mode (switch_value == 0x03)
				spiWrite(MAX7219_DIGIT8_ADDR,				1);
				setup_dc_measure();
			}
		}
		prev_switch_value = switch_value;
	}
}  // end main
