#include <ADUC841.H>	// special function register definitions
#include "typedef.h"	// variable type definitions
#include "main.h"

void timer0_isr(void) interrupt 1 {
	static uint16 heartbeat_counter = 0; // Used to divide ISR frequency so LED blinks slowly. Static so it persists across function calls

	// This is used for the heartbeat LED blinking. Just toggle the LED and clear the interrupt flag
	heartbeat_counter++;

	if (heartbeat_counter == 50) {
		HEARTBEAT_LED = ~HEARTBEAT_LED | HEARTBEAT_SWITCH; // Toggle LED (unless bit 5 of SWITCHES is low)
		heartbeat_counter = 0;
	}	TF0 = 0; // clear interrupt flag
}

void setup_heartbeat(void){
	// Setup timer 0 to be used for heartbeat LED blinking
	//TMOD = (0 << GATE_pos) | // Timer 0 gate control
	//       (0 << CT_pos)   | // Timer/counter mode
	//       (1 << M1_pos)   | // Mode bit 1 
	//       (0 << M0_pos);    // Mode bit 0
	// TODO
	TMOD = 0x51;
	// TODO use defined bit positions here and resolve conflict with configuration for T1 in frequency measure mode

	TCON =	(0 << TF1_pos) | // Timer 1 overflow flag off
					(0 << TR1_pos) | // Timer 1 should be running
					(0 << TF0_pos) | // Timer 0 overflow flag
					(1 << TR0_pos) | // Timer 0 run control
					(0 << IE1_pos) | // External interrupt 1 flag
					(0 << IT1_pos) | // External interrupt 1 type
					(0 << IE0_pos) | // External interrupt 0 flag
					(0 << IT0_pos);   // External interrupt 0 type
}
