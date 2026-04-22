/*--------------------------------------------------------------------------------------------------
	Demonstration program for Cortex-M0 SoC design - basic version, no CMSIS

	Enable the interrupt for UART - interrupt when a character is received.
	Repeat:
	  Set the LEDs to match the switches, then flash the 8 rightmost LEDs.
	  Go to sleep mode, waiting for an interrupt
		  On UART interrupt, the ISR sends the received character back to UART and stores it
			  main() also shows the character code on the 8 rightmost LEDs
	  When a whole message has been received, the stored characters are copied to another array
		  with their case inverted, then printed. 

	Version 6 - March 2023
  ------------------------------------------------------------------------------------------------*/

#include <stdio.h>					// needed for printf
#include "DES_M0_SoC.h"			// defines registers in the hardware blocks used

#define BUF_SIZE						100				// size of the array to hold received characters
#define ASCII_CR						'\r'			// character to mark the end of input
#define CASE_BIT						('A' ^ 'a')		// bit pattern used to change the case of a letter
#define FLASH_DELAY					1000000		// delay for flashing LEDs, ~220 ms

#define INVERT_LEDS					(GPIO_LED ^= 0xff)		// inverts the 8 rightmost LEDs

#define ARRAY_SIZE(__x__)   (sizeof(__x__)/sizeof(__x__[0]))  // macro to find array size

// Global variables - shared between main and UART_ISR
volatile uint8  RxBuf[BUF_SIZE];	// array to hold received characters
volatile uint8  counter  = 0; 		// current number of characters in RxBuf[]
volatile uint8  BufReady = 0; 		// flag indicates data in RxBuf is ready for processing


//////////////////////////////////////////////////////////////////
// Interrupt service routine, runs when UART interrupt occurs - see cm0dsasm.s
//////////////////////////////////////////////////////////////////
void UART_ISR()		
{
	char c;
	c = UART_RXD;	 				// read character from UART (there must be one waiting)
	RxBuf[counter]  = c;  // store in buffer
	counter++;            // increment counter, number of characters in buffer
	UART_TXD = c;  				// write character to UART (assuming transmit queue not full)

	/* Counter is now the position in the buffer that the next character should go into.
		If this is the end of the buffer, i.e. if counter == BUF_SIZE-1, then null terminate
		and indicate that a complete sentence has been received.
		If the character just put in was a carriage return, do the same.  */
	if (counter == BUF_SIZE-1 || c == ASCII_CR)  
	{
		counter--;							// decrement counter (CR will be over-written)
		RxBuf[counter] = NULL;  // null terminate to make the array a valid string
		BufReady       = 1;	    // indicate that data is ready for processing
	}
}


//////////////////////////////////////////////////////////////////
// Interrupt service routine for System Tick interrupt
//////////////////////////////////////////////////////////////////
void SysTick_ISR()
{
	// Do nothing - this interrupt is not used here
}


//////////////////////////////////////////////////////////////////
// Software delay function - delay time proportional to argument n
// As a rough guide, delay(1000) takes 160 us to 220 us, 
// depending on the compiler optimisation level.
//////////////////////////////////////////////////////////////////
void delay (uint32 n) 
{
	volatile uint32 i;
		for(i=0; i<n; i++);		// do nothing n times
}

void write_accelerometer_register(uint8 addr, uint8 data) {
	uint32 write_val;
	
	write_val = ((0x0A << 16) | // write instruction
							 (addr <<  8) | // address
							 (data));
	
	GPIO_ACC = 0x00; // chip select (active low)
	
	//printf("\tWriting to SPIDAT: [%8x] <= %8x\n",addr,write_val);

	pt2SPIDAT = write_val;
	
	//printf("\tWrote SPIDAT, waiting for busy flag to clear\n");
	while(1) {
		uint32 spicon_rd;
		spicon_rd = pt2SPICON; // read spicon
		//printf("SPICON: %8x",spicon_rd);
		if ((spicon_rd & 0x40000000)) { // while we are busy
			//printf("."); // busy
		} else {
			break; // break once the 'busy' flag drops
		}
	}
	//printf("\tSPICON 'busy' flag dropped\n");
	
	GPIO_ACC = 0xFF; // deselect chip
}

uint32 read_accelerometer_register(uint8 addr) {
	uint32 read_val,write_val;
	
	write_val = ((0x0B << 16) | // read instruction
							 (addr <<  8) | // address
							 (0x00)); // just write zeros - we are interested in the return value only
	
	//printf("\tReading SPIDAT: [%8x] <= %8x\n",addr,write_val);
	GPIO_ACC = 0x00; // chip select (active low)
	pt2SPIDAT = write_val;
	read_val = pt2SPIDAT; // TODO REMOVE THIS we shouldn't be reading back so quickly but it's usefulf or debugging
	//printf("\tImmediately read back %8x\n",read_val);
	
	//printf("\tWrote SPIDAT, waiting for busy flag to clear\n");
	while(1) {
		uint32 spicon_rd;
		spicon_rd = pt2SPICON; // read spicon
		//printf("SPICON: %8x",spicon_rd);
		if ((spicon_rd & 0x40000000)) { // while we are busy
			//printf("."); // busy
		} else {
			break; // break once the 'busy' flag drops
		}
	}
	//printf("\tSPICON 'busy' flag dropped\n");
	
	GPIO_ACC = 0xFF; // deselect chip
	
	read_val = pt2SPIDAT; // read SPIDAT back TODO extract only relevant bits
	//printf("\tRead %8x from SPIDAT\n",read_val);
	
	return read_val; // read SPIDAT back
}

///////////////////
// Configure the accelerometer
void configure_accelerometer() { // ADXL362
	volatile int i;
	//printf("Configuring accelerometer\n");
	write_accelerometer_register(0x1F,0x52); // 0x52 is the instruction to reset ('R' in ASCII)
	
	//delay(FLASH_DELAY);
	
	//write_accelerometer_register(0x20, 250);
	//write_accelerometer_register(0x21, 0);
	//write_accelerometer_register(0x22, 2);
	//write_accelerometer_register(0x23, 0x96);
	//write_accelerometer_register(0x24, 0);
	//write_accelerometer_register(0x25, 0x1e);
	//write_accelerometer_register(0x26, 0);
	//write_accelerometer_register(0x27, 0x3f);
	//write_accelerometer_register(0x2b, 0x40);
	write_accelerometer_register(0x2d, 0x2);
	
	

	//printf("Sent reset to accelerometer - delaying\n");
	//for(i = 0; i<100000; i++); // "a latency of 0.5 ms is required after soft reset" I have no IDEA if this is the right duration TODO
	//printf("Delay complete\n");
	// TODO include these lines of code for synthesis
	
	//printf("Sending config to accelerometer\n");
	//write_accelerometer_register(0x2D,2); // set mode to measurement mod
	//printf("Config sent\n");
}

//////////////////////////////////////////////////////////////////
// Main Function
//////////////////////////////////////////////////////////////////
int main(void) 
{
	uint32 accelorometer_value;
	int i;
	
	GPIO_LED = 0x1;
	
	NVIC_Disable = (0xFFFFFFFF);	// disable all interrupts
	GPIO_LED = 0x5;
	pt2DISPMODE = 0xFF;
	GPIO_LED = 0x6;
	pt2DIGITENABLE = 0xff;
	GPIO_LED = 0x7;
	
	i=0;
	while(1) {
		pt2DISP->digit0 = 0x1;
		pt2DISP->digit1 = 0x2;
		pt2DISP->digit2 = 0x4;
		pt2DISP->digit3 = 0x8;
		pt2DISP->digit4 = 0x10;
		pt2DISP->digit5 = 0x20;
		pt2DISP->digit6 = 0x40;
		pt2DISP->digit7 = 0x80;
		i=i+1;
		delay(FLASH_DELAY);
	}


	GPIO_LED = 0x2;
	
	
	//printf("\n\nWelcome to Cortex-M0 SoC version 2\n");	 // print a welcome message
	// printf doesn't work in simulation because the UART isn't modelled correctly
	
	// Set up the SPI device
	GPIO_ACC = 0xFF;
	pt2SPICON = (0 << 6) | (6 <<3) | (0 << 2) | (3);
	GPIO_LED = 0x4;

	configure_accelerometer();

	while(1) {
		//printf("\nReading address %8x\n",j);
		uint16 sw;
		sw = GPIO_SW;
		accelorometer_value = read_accelerometer_register(sw);
		GPIO_LED = accelorometer_value;
		//pt2DISP0 = accelorometer_value & 0xf;
		//pt2DISP1 = accelorometer_value & 0xf0;
		//pt2DISP2 = accelorometer_value & 0xf00;
		//pt2DISP3 = accelorometer_value & 0xf000;
		//pt2DISP4 = accelorometer_value & 0xf0000;
		//pt2DISP5 = accelorometer_value & 0xf00000;
		//pt2DISP6 = accelorometer_value & 0xf000000;
		//pt2DISP7 = accelorometer_value & 0xf0000000;
		//delay(10000);
	}


}  // end of main
