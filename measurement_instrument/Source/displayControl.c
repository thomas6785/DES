#include <ADUC841.H>	// special function register definitions
#include "typedef.h"	// variable type definitions
#include "main.h"

void delay() {
	// Delay for ~1000 clocks e.g. to allow SPI transaction to complete
	uint8 k;
 	for(k=0; k != 255; k++);
}

void humanScaleDelay() {
	uint8 i,j,k;
	// Approximately a ~2 second software delay
	for(i=0; i != 75; i++)
		for(j=0; j != 255; j++)
			for(k=0; k != 255; k++);
}

void spiWrite(uint8 address, uint8 data_value) {
	SPI_LOAD = 0;				// load goes low at start of a new SPI transaction

	// Write the address byte
	SPIDAT = address;		// send address byte (writing to SPIdat will trigger the SPI transmission too)
	while(~ISPI);				// wait until ISPI is 1 indicating the SPI write is done
	ISPI = 0;						// reset ISPI

	// Need a small delay between SPI transactions
	delay(); // TODO check this is long enough or if it can be any shorter

	// Write the data byte
	SPIDAT = data_value;	// send data byte
	while(~ISPI);					// wait until ISPI is 1 indicating the SPI write is done
	ISPI = 0;							// reset ISPI

	SPI_LOAD = 1;					// load goes high at end to tell the peripheral to update

	delay(); // TODO check this is long enough or if it can be any shorter
}

void displayValue(uint32 value) {
	// Takes in a 32-bit binary value and convert to binary-coded decimal, then
	// display on the 7-segment display
	uint8 i;
	uint32 bcd; // Holds the BCD digits, each one nybble. Stored as a 32-bit value to make bitshifting easier

	//uint8 sign; // Assume input is always positive for now, so sign bit is 0
	// If the value is negative, flip the sign bit and make the value positive for the BCD conversion
	//if (value < 0) {
	//	sign = 1;
	//	value = -value;
	//} else {
	//	sign = 0;
	//}

	bcd = 0;

	for (i = 0; i<32; i++) { // TODO the first few iterations of this loop are useless, skip them somehow
		// if any BCD digit has more than 5, increment it by 3
		if (i != 0) { // if statement so we can skip this the first time round
			if ((bcd & 0xF00000) > 0x400000) { bcd += 0x300000; }
			if ((bcd & 0x0F0000) > 0x040000) { bcd += 0x030000; }
			if ((bcd & 0x00F000) > 0x004000) { bcd += 0x003000; }
			if ((bcd & 0x000F00) > 0x000400) { bcd += 0x000300; }
			if ((bcd & 0x0000F0) > 0x000040) { bcd += 0x000030; }
			if ((bcd & 0x00000F) > 0x000004) { bcd += 0x000003; }
		}

		// shift a bit into the BCD array from the input value
		bcd = bcd << 1;
		bcd = bcd | (0x01 & (value >> 31));	// Move the MSB of bcd into the LSB to create a carry bit for the next shift
		value = value << 1;								// Shift input value left by 1 to get the next bit into the MSB
	}

	if (SCALE_UNITS_SWITCH) { // if we are scaling units Hz->kHz mV-V, then
		bcd = bcd + 0xB00; // Add B to the most significant nybble being cut off to give the effect of rounding to the nearest value
		bcd = bcd >> 12; // Cut off the last 12 bits to remove the last 3 decimal digits
	}
	
	// Write the BCD to the display using SPI
	spiWrite(MAX7219_DIGIT1_ADDR,		((bcd & 0x00000F)>>0) );	// Tenths
	spiWrite(MAX7219_DIGIT2_ADDR,		((bcd & 0x0000F0)>>4) | 0x80);	// Ones. MSB is set to display a decimal point
	spiWrite(MAX7219_DIGIT3_ADDR,		((bcd & 0x000F00)>>8) );	// Tens
	spiWrite(MAX7219_DIGIT4_ADDR,		((bcd & 0x00F000)>>12) );	// Hundreds
	spiWrite(MAX7219_DIGIT5_ADDR,		((bcd & 0x0F0000)>>16) );	// Thousands
	spiWrite(MAX7219_DIGIT6_ADDR,		((bcd & 0xF00000)>>20) );	// Ten thousands
	// Hopefully the compiler can recognise that we don't need to apply the & to all of these since they are separate in memory
	// Yep it does! great
}

void initialDisplaySetup() { // TODO functions like this are only called once so should be inlined, but the uVision compiler doesn't have support for inlining. The suggested alternative is to use a macro or just take the hit on the overhead
	SPICON =	(0 << ISPI_pos)	|
	 					(0 << WCOL_pos)	|
	 					(1 << SPE_pos)	|
	 					(1 << SPIM_pos)	|
	 					(0 << CPOL_pos)	|
	 					(0 << CPHA_pos)	|
	 					(3 << SPR_pos); // TODO check if we can make the SPI clock faster, we probably can

	// Run a display test
	spiWrite(MAX7219_DISPLAY_TEST_ADDR,	1);		// Display test register will enable all LEDs
	humanScaleDelay();												// Delay for the display test to be visible to humans
	spiWrite(MAX7219_DISPLAY_TEST_ADDR,	0);		// Display test register will enable all LEDs

	// Set up the display for normal operation
	spiWrite(MAX7219_SHUTDOWN_ADDR,			1);			// Switch on the display
	spiWrite(MAX7219_DECODE_MODE_ADDR,	0xFF);	// Set to '1' to use a LUT to display digits using the 7 segment display
	spiWrite(MAX7219_INTENSITY_ADDR,		15);		// Set to 15 for max brightnes
	spiWrite(MAX7219_SCAN_LIMIT_ADDR,		5);			// Set to 5 for 6 digits on display
}

extern uint32 iir_value;
extern uint8 current_mode; // TODO not sure 'extern' is a clean solution for this, might be better to put these in the header file

void updateDisplay() {
	uint32 scaled_value;
	static uint32 previous_iir_value = 0;
	uint32 current_iir_value;

	EA = 0; // Disable interrupts while we read the IIR value to avoid it changing while we copy to a local variable
	current_iir_value = iir_value; // Read the current value of the IIR filter
	EA = 1; // Re-enable interrupts
	
	if (current_iir_value != previous_iir_value) { // 'if' statement so we can skip updating the display if nothing has changed
		if (current_mode == FREQUENCY_MODE) {
			scaled_value = (current_iir_value >> (IIR_HIDDEN_PRECISION_BITS-8));
			// there are LSB's of hidden precision in the IIR filter to be right-shifted out
			// however to scale appropriately we multiply by 256 (<<8) which gives the answer in multiples of 0.1 Hz
		}
		else if (current_mode == AMPLITUDE_MODE) {
			scaled_value = ((((current_iir_value * 7)>>IIR_HIDDEN_PRECISION_BITS))*7)>>2;
			// TODO refactor this to try use less bitwidth and add comments explaining the calculation, why we split 49=7*7, and why we divide by 4 (>>2)
		}
		else if (current_mode == DC_MODE) {
			scaled_value = (((current_iir_value * 7)>>IIR_HIDDEN_PRECISION_BITS)*7)>>3; // TODO justify calculation of 49/8 in comments
			// TODO refactor this to try use less bitwidth + comments as above needed
		}
		displayValue(scaled_value); // Update the display with the current value of the IIR filter
		previous_iir_value = current_iir_value;
	}
}
