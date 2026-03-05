#include <ADUC841.H>	// special function register definitions
#include "typedef.h"	// variable type definitions
#include "main.h"

static uint16 y_max =0x00; // For amplitude measurement, we need to keep track of the maximum value in the sampling window
static uint16 y_min = 0xFFFF; // For amplitude measurement, we also need to keep track of the minimum value in the sampling window

uint8 current_mode; // the mode we are in currently

sfr16 ADCDATA = 0xD9; // TODO why???

void setup_interrupts_and_timers() { // TODO can't we make all these functions 'inline'? Compiler seems not to like the 'inline' keyword
	// Configure the interrupts we are using
	IE =	(1 << EA_pos ) | // global interrupt enable bit
				(0 << EADC_pos ) |
				(1 << ET2_pos ) | // keep interrupts from timer 2, they are used to update the display in all modes
				(0 << ES_pos ) |
				(0 << ET1_pos ) |
				(0 << EX1_pos ) |
				(1 << ET0_pos ) | // keep interrupts from timer 0, they are used for the heartbeat LED in all modes
				(0 << EX0_pos );

	// Configure T2 to have priority
	IP =	(0 << EA_pos ) | // bit is not used for the interrupt priority register
				(0 << EADC_pos ) |
				(1 << ET2_pos ) | // T2 has higher priority than T1 - we don't want the blinking LED to interfere with our measurements
				(0 << ES_pos ) |
				(0 << ET1_pos ) |
				(0 << EX1_pos ) |
				(0 << ET0_pos ) |
				(0 << EX0_pos );

	// Clear counters to remove leftovers when switching modes
	TH1 = 0; TL1 = 0;
	TH2 = 0; TL2 = 0;

	// Turn off the display test, if it was on from the display test mode
	spiWrite(MAX7219_DISPLAY_TEST_ADDR,	0);
}

void timer2setup(void) {
	// In all modes, timer 2 is used to generate an ISR every 125 ms
	// which is used to update the display value
	
	// Configure timer 2 to generate an interrupt
	T2CON = (0 << TF2_pos)     | // Overflow flag
	        (0 << EXF2_pos)    | // External flag
	        (0 << RCLK_pos)    | // receive flag enable
	        (0 << TCLK_pos)    | // transmit clock enable
	        (0 << EXEN2_pos)   | // external enable flag
	        (1 << TR2_pos)     | // start/stop bit
	        (0 << CNT2_pos)    | // Timer/counter mode
	        (0 << CAP2_pos);     // Capture/reload mode

	RCAP2L = 0x10;
	RCAP2H = 0x2D;
	// set reload value to 0x2D10
	// in frequency mode we divide this by 8 to get a sampling window of 39.0625 milliseconds (such that we can multiply by 256 to get our frequency in multiples of 0.1 Hz)
	// in amplitude mode we divide this by 32 to get a sampling window of 0.15625 seconds (long enough to capture the peak and trough of any signal many times)
}

/*------------------------------------------------
Interrupt service routine for timer 2 interrupt.
Called by the hardware when the interrupt occurs.
------------------------------------------------*/
void timer2_isr(void) interrupt 5 { // interrupt vector at 002BH
	int16 new_measurement_value;
	// TODO split each mode of operation into a separate function here (they can be inlined by compiler) tidy this function generally
	
	static uint8 timer2_interrupt_count = 0; // tracks how many ISR's have occured. We only respond to every 8th ISR so we are updating every 10/256 seconds
	// static variable so it will persist across function calls

	timer2_interrupt_count++;
	if (timer2_interrupt_count == 8) {
		if (current_mode == FREQUENCY_MODE) {
			timer2_interrupt_count = 0;
			new_measurement_value = (TH1 << 8) | TL1; // Read the value of timer 1, which is counting the edges on the input signal
			// TODO check if this has overflowed the max frequency
			TH1 = 0; TL1 = 0; // Reset timer 1 for the next measurement
			feed_iir(new_measurement_value); // update IIR filter with the new measured value
		}

		if (current_mode == AMPLITUDE_MODE) {
			timer2_interrupt_count = 0;
			feed_iir(get_amplitude_measurement()); // update IIR filter with the new measured value
		}
	}

	TF2 = 0; // Clear the interrupt flag
}  // end timer2 ISR

void adc_isr(void) interrupt 6 {
	uint16 sample_value = (ADCDATAH <<8 | ADCDATAL) & 0x0FFF;
	if (current_mode == DC_MODE) {
		// IIR Filter
		feed_iir(sample_value);
	} else if (current_mode == AMPLITUDE_MODE) {
		amplitude_handle_sample(sample_value);
	}
}

void write_status_leds(void) {
	//P0 bits 0,1 represent the mode in binary, so we can just write the value of the mode to those bits to display it on the LEDs
	//P0 bits 4,5,6, and 7 represent what measurement is being displayed (Hz, KHz, V or mV) depending on SCALE_UNITS_SWITCH and the mode of the device.
	uint8 lights = 0;
	switch (current_mode) {
		case DC_MODE: lights						= lights | 0x01; break;
		case FREQUENCY_MODE: lights			= lights | 0x02; break;
		case AMPLITUDE_MODE: lights			= lights | 0x04; break;
		case DISPLAY_TEST_MODE: lights	= lights | 0x08; break;
	}

	if ((current_mode == FREQUENCY_MODE) && ((SCALE_UNITS_SWITCH) == 0x00)) {
		lights = lights | (1 << 4); // To display Hz
	} else if ((current_mode == FREQUENCY_MODE) && ((SCALE_UNITS_SWITCH) == 0x01)) {
		lights = lights | (1 << 5); // To display KHz
	} else if ((current_mode == DC_MODE || current_mode == AMPLITUDE_MODE) && ((SCALE_UNITS_SWITCH) == 0x00)) {
		lights = lights | (1 << 6); // To display mV
	} else if ((current_mode == DC_MODE || current_mode == AMPLITUDE_MODE) && ((SCALE_UNITS_SWITCH) == 0x01)) {
		lights = lights | (1 << 7); // To display V
	} else {
		lights = lights & 0x0F; // Clear bits 4 to 7 if we are in a mode where they aren't used
	}

	P0 = ~lights; // invert because the LED output is active low
}

void setup_display_test(void) {
	spiWrite(MAX7219_DISPLAY_TEST_ADDR,	1);
	// write to the display to turn on the display test mode, which should light up all the LEDs on the peripheral display. This is used for testing that the display is working and also to show users that the device is on and working when they switch it on.
}

void main(void) {
	uint8 perv_switches;
	uint8 current_switches;
	uint16 display_update_divider;
	
	display_update_divider = 0;

 	initialDisplaySetup(); // set up the SPI display, including a display test long enough for humans to see all LEDs light up
	timer2setup();
	setup_heartbeat(); // Start the heartbeat LED blinking to show that the system is alive and running
	setup_interrupts_and_timers();

	// Configure switches for user input and begin reading their values
	SWITCHES = 0xFF;  // Make switch pins inputs
	T1 = 1; // put timer 1 counter input in input mode  TODO why is this in main??
	perv_switches = 0x8;

	while(1) {
		// Read switches and enter appropriate mode
		// TODO I think this could all be a bit cleaner
		current_switches = SWITCHES ;
		current_mode = MODE_SWITCHES;
		if (perv_switches != current_switches) {
			write_status_leds();
			setup_interrupts_and_timers(); // Set interrupts and timers back to their default configurations
			reset_iir(); // Reset the IIR filter for the display when we switch modes (this will set a flag that causes it to overwrite the IIR value on the next update, then return to normal IIR operation)
			// TODO IIR shouldn't reset on unit switch or other switches that are not mode switches
			
			// TODO use a switch case statement here
			if (current_mode == DC_MODE) {
				setup_dc_measure();
			} else if (current_mode == FREQUENCY_MODE) {
				setup_frequency_measure(); 
			} else if (current_mode == AMPLITUDE_MODE) {
				setup_amplitude_measure();
			} else if (current_mode == DISPLAY_TEST_MODE) {
				setup_display_test();
			}
		}
		perv_switches = current_switches;
		
		// Update the display every x loops (throttled to avoid flickering)
		display_update_divider++;
		if ((display_update_divider == 30000) & ~(FREEZE_SWITCH)) {
			display_update_divider = 0;
			updateDisplay();
		}
	}
}  // end main
// TODO make sure that IIR isn't reset if we toggle a non-mode swt