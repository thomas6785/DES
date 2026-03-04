#include <ADUC841.H>	// special function register definitions
#include "typedef.h"	// variable type definitions
#include "main.h"

void delay() {
	// Delay for ~1000 clocks e.g. to allow SPI transaction to complete
	// TODO implement this in ASM instead - compiler bloats
	
	uint8 k;
 	for(k=0; k != 255; k++);
}

void humanScaleDelay() {
	uint8 i,j,k;
	// Approximately a ~2 second software delay

	// TODO implement this in ASM instead - slightly more compact
	// TODO why is this in displayControl?

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
	// TODO remove this delay - not clear if it is necessary or not? but 10k is definitely too many cycles
}

void displayValue(int16 value) {
	// Take in a 16-bit binary value and convert to binary-coded decimal, then
	// display on the 7-segment display
	uint8 i;
	uint8 bcd[3]; // Array to hold the BCD digits. bcd[0] is the ones and tens, bcd[1] is the hundreds and thousands, etc.
	//uint8 sign; // Assume input is always positive for now, so sign bit is 0 TODO use something smaller than a char
	// TODO maybe just use a 32-bit word instead of 3 8-bit words since the compiler will be smarter about bitshifting then
	// If the value is negative, flip the sign bit and make the value positive for the BCD conversion
	//if (value < 0) {
	//	sign = 1;
	//	value = -value;
	//} else {
	//	sign = 0;
	//}

	bcd[0] = 0; // Ones and tens
	bcd[1] = 0; // hundreds and thousands
	bcd[2] = 0; // tens of thousands TODO merge sign bit into MSB of this byte

		// TODO rewrite this code in Assembly, particularly the bitshifting operations are MUCH more efficient using rotations through the carry bit
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
	spiWrite(MAX7219_DIGIT2_ADDR,		(bcd[0] & 0xF0) >> 4 | 0x80);	// Tens. MSB is set to display a decimal point
	spiWrite(MAX7219_DIGIT3_ADDR,		bcd[1] & 0x0F);	// Hundreds
	spiWrite(MAX7219_DIGIT4_ADDR,		(bcd[1] & 0xF0) >> 4);	// Thousands
	spiWrite(MAX7219_DIGIT5_ADDR,		bcd[2] & 0x0F);	// Ten thousands
	// TODO also write a sign bit to the display
	// TODO and maybe write to the more significant digits
	// TODO need logic to move three decimal places over to switch from Hz to kHz or mV to V
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
	spiWrite(MAX7219_DIGIT1_ADDR,				0);			// Initially display all 0's // TODO can probably remove this as we will immediately overwrite it
	spiWrite(MAX7219_DIGIT2_ADDR,				0);			// Initially display all 0's
	spiWrite(MAX7219_DIGIT3_ADDR,				0);			// Initially display all 0's
	spiWrite(MAX7219_DIGIT4_ADDR,				0);			// Initially display all 0's
	spiWrite(MAX7219_DIGIT5_ADDR,				0);			// Initially display all 0's
	spiWrite(MAX7219_DIGIT6_ADDR,				0);			// Initially display all 0's
	spiWrite(MAX7219_DIGIT7_ADDR,				0);			// Initially display all 0's
	spiWrite(MAX7219_DIGIT8_ADDR,				0);			// Initially display all 0's
	spiWrite(MAX7219_INTENSITY_ADDR,		15);		// Set to 15 for max brightness (for now TODO set brightness appropriately)
	spiWrite(MAX7219_SCAN_LIMIT_ADDR,		7);			// Set to 7 for 8 digits for now TODO we may not need all 8 digits
}

extern uint16 iir_value;
extern uint8 current_mode; // TODO not sure 'extern' is a clean solution for this, might be better to put these in the header file

void updateDisplay() {
	uint16 scaled_value;
	static previous_iir_value = 0;
	
	if (iir_value != previous_iir_value) { // 'if' statement so we can skip updating the display if nothing has changed
		if (current_mode == FREQUENCY_MODE) {
			// TODO scale and shift the frequency value appropriately before displaying
			scaled_value = iir_value;
		}
		else if (current_mode == AMPLITUDE_MODE) {
			scaled_value = ((iir_value*7)>>2)*7;
		}
		else if (current_mode == DC_MODE) {
			scaled_value = ((iir_value*7)>>3)*7; // TODO justify "7/8 * 7" calculation in comments
		}
		displayValue(scaled_value); // Update the display with the current value of the IIR filter
		previous_iir_value = iir_value;
	}
}
