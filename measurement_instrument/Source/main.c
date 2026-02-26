#include <ADUC841.H>	// special function register definitions
#include "typedef.h"	// variable type definitions
// ^ where is this file?
// can we delete MOD841 then? TODO

// TODO should probably move these definitions to another file
#define SWITCHES  	P2		// switches are connected to Port 2
#define LOAD 				RXD

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

// SPI Con
#define ISPI_pos				(7) // Interrupt bit
#define WCOL_pos				(6) // Write collision bit
#define SPE_pos					(5) // SPI interface enable
#define SPIM_pos				(4) // set for master, clear for slave mode
#define CPOL_pos				(3) // set for clock to idle high, clear for idle low
#define CPHA_pos				(2) // clock polarity. Set for leading edge to transmti, clear for trailing edge to transmit
#define SPR_pos					(0) // select SCLCK rate - see data sheet

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
	TF2 = 0;
}  // end timer2 interrupt service routine

void spiWrite(uint8 address, uint8 data_value)
{
	int i;

	LOAD = 0;						// load goes low at start

	// Write the address byte
	SPIDAT = address;		// send address byte (writing to SPIdat will trigger the SPI transmission too)
	while(~ISPI){}			// wait until ISPI is 1 indicating the SPI write is done
	ISPI = 0;						// reset ISPI

	// Need a small delay between SPI transactions
	for(i=0; i <=10000; i++){} // TODO shorten delay - not clear if it is necessary or not?

	// Write the data byte
	SPIDAT = data_value;	// send data byte
	while(~ISPI){}				// wait until ISPI is 1 indicating the SPI write is done
	ISPI = 0;							// reset ISPI

	LOAD = 1;							// load goes high at end to tell the peripheral to update

	for(i=0; i <=10000; i++); //Delay
	// TODO remove this delay - not clear if it is necessary or not?
}

void display_value(uint16 value) // TODO should be int not uint
{
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
}

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
	int i;
	uint8 prev_switch_value;


	SPICON =	(0 << ISPI_pos)	|
						(0 << WCOL_pos)	|
						(1 << SPE_pos)	|
						(1 << SPIM_pos)	|
						(0 << CPOL_pos)	|
						(1 << CPHA_pos)	|
						(3 << SPR_pos);

	display_value(65244);
	display_value(999);
	display_value(-999);
	display_value(32767);
	display_value(-32768);
	
	spiWrite(MAX7219_DISPLAY_TEST_ADDR,	1);		// Run display test
	for(i=0; i <=1000000; i++); //Delay for the display test to be visible to humans
	spiWrite(MAX7219_DISPLAY_TEST_ADDR,	0);		// Disable display test

	spiWrite(MAX7219_SHUTDOWN_ADDR,			1);		// Switch on the display
	spiWrite(MAX7219_DIGIT1_ADDR,				1);		// Some digits to display
	//spiWrite(MAX7219_DIGIT2_ADDR,				2);		//
	//spiWrite(MAX7219_DIGIT3_ADDR,				3);		//
	//spiWrite(MAX7219_DIGIT4_ADDR,				4);		//
	//spiWrite(MAX7219_DIGIT5_ADDR,				5);		//
	//spiWrite(MAX7219_DIGIT6_ADDR,				6);		//
	//spiWrite(MAX7219_DIGIT7_ADDR,				7);		//
	//spiWrite(MAX7219_DIGIT8_ADDR,				8);		//
	spiWrite(MAX7219_DECODE_MODE_ADDR,	1);		// Set to '1' to use a LUT to display digits using the 7 segment display
	spiWrite(MAX7219_INTENSITY_ADDR,		15);	// Set to 15 for max brightness (for now TODO)
	spiWrite(MAX7219_SCAN_LIMIT_ADDR,		7);		// Set to 7 for 8 digits

	// Configure switches for user input and begin reading their values
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
