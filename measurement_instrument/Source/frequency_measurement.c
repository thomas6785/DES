#include <ADUC841.H>	// special function register definitions
#include "typedef.h"	// variable type definitions
#include "main.h"

void setup_frequency_measure(void) {
	// Frequency measurement mode.
	// Uses timer 2 to generate an ISR every 0.1 seconds
	// and timer 0 to count positive edges on the input signal
	// every 0.1 s, the value in timer 0 is read and displayed (via the IIR filter),
	// then timer 0 is reset

	// Configure timer 0 to count the input frequency. Use in mode 1.
	//TMOD = (0 << GATE_pos) | // Timer 0 gate control
	//       (1 << CT_pos)   | // Timer/counter mode
	//       (0 << M1_pos)   | // Mode bit 1
	//       (1 << M0_pos);    // Mode bit 0
	TMOD = 0x51; //0b00000101; // look at diagram on page 70 for mode 1. Gate is LOW so that we are always counting, and timer is connected to P3.5 TODO use defined bit positions here

	TCON =	(0 << TF1_pos) | // Timer 1 overflow flag off
					(1 << TR1_pos) | // Timer 1 should be running
					(0 << TF0_pos) | // Timer 0 overflow flag
					(1 << TR0_pos) | // Timer 0 run control
					(0 << IE1_pos) | // External interrupt 1 flag
					(0 << IT1_pos) | // External interrupt 1 type
					(0 << IE0_pos) | // External interrupt 0 flag
					(0 << IT0_pos);   // External interrupt 0 type
}
