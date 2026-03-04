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

static uint8 current_mode;

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
	TH0 = 0; TL0 = 0;
	TH2 = 0; TL2 = 0;
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

	// Set reload value to 1536
	// This gives an ISR every 64,000 clock cycles
	// which is then divided by 27 so the display is updated every 10/64 seconds
	RCAP2L = 0x00;
	RCAP2H = 0x06;
}

/*------------------------------------------------
Interrupt service routine for timer 2 interrupt.
Called by the hardware when the interrupt occurs.
------------------------------------------------*/
void timer2_isr(void) interrupt 5 { // interrupt vector at 002BH
	uint16 new_measurement_value;
	// TODO split each mode of operation into a separate function here (they can be inlined by compiler)
	
	static uint8 timer2_interrupt_count = 0; // tracks how many ISR's have occured. We only respond to every 27th ISR so we are updating every 10/64 seconds
	// static variable so it will persist across function calls

	timer2_interrupt_count++;
	if (timer2_interrupt_count == 27) {
		timer2_interrupt_count = 0;

		if (current_mode == FREQUENCY_MODE) {
			new_measurement_value = (TH1 << 8) | TL1; // Read the value of timer 1, which is counting the edges on the input signal
			// TODO scale and shift
			TH1 = 0; TL1 = 0; // Reset timer 1 for the next measurement
		}
		else if (current_mode == AMPLITUDE_MODE) {
			new_measurement_value = y_max - y_min; // Get the amplitude value for the sampling window
			// TODO scale and shift

			// Reset the max and min for the next sampling window
			y_max = 0x00;
			y_min = 0xFFFF;
		}
		else if (current_mode == DC_MODE) {
			new_measurement_value = ADCDATA & 0x0FFF; // TODO scale and shift
			// TODO check if this is the correct way to read ADCDATA
			// TODO check that ADCDATA is always valid for reading at any time
		}
		update_display_via_iir(new_measurement_value); // update the display with the new measured value
	} // TODO move handling for each individual mode to a separate function for readability

	TF2 = 0; // Clear the interrupt flag
}  // end timer2 ISR

void adc_isr(void) interrupt 6 {
	uint16 sample_value = (ADCDATAH <<8 | ADCDATAL) & 0x0FFF; //TODO is this correct?
	/*if ((SWITCHES&0x03) == 0x00) { // TODO I don't like that we are reading the switches directly in the ISR
		// DC measurement mode. Take the sample and update the IIR filter output
		// IIR Filter
		sample_value = sample_value & 0x0FFF;
		update_display_via_iir(sample_value);

	} else*/ if (current_mode == AMPLITUDE_MODE) { // TODO edge case hazard here: if we only just switched into amplitude mode, it may not be set up yet, so the ADC data could be old
		// Record the highest and lowest value
		if (sample_value > y_max){
			y_max = sample_value;
		}

		if (sample_value < y_min){
			y_min = sample_value;
		}
	}
}

void timer0_isr(void) interrupt 1 {
	static uint16 heartbeat_counter = 0; // Used to divide ISR frequency so LED blinks slowly. Static so it persists across function calls

	// This is used for the heartbeat LED blinking. Just toggle the LED and clear the interrupt flag
	heartbeat_counter++;

	if (heartbeat_counter == 50) {
		HEARTBEAT_LED = ~HEARTBEAT_LED & HEARTBEAT_SWITCH; // Toggle LED (unless bit 5 of SWITCHES is low)
		heartbeat_counter = 0;
	}
	TF0 = 0; // clear interrupt flag
}

void setup_dc_measure(void) {
	// Configure ADC for DC
	ADCCON1 = (1 << MD1_pos)    | // Operating mode of ADC
	          (0 << EXT_REF_pos)| // External reference
	          (0 << CK1_pos)    | // ADC clock divide bits
	          (0 << CK0_pos)    | // ADC clock divide bits
	          (3 << AQ_pos)     | // number of ADC clock cycles
	          (1 << T2C_pos)    | // trigger ADC conversion on timer 2 overflow
	          (0 << EXC_pos);     // external trigger off

	// Enable ADC interrupt // TODO this is wrong, we don't need to set the flag ourselves here
	ADCCON2 = (0 << ADCI_pos)   | // 0: clear ADC interrupt flag
	          (0 << DMA_pos)    | // 0: not using DMA
	          (0 << CCONV_pos)  | // 0: disable continuous conversion
	          (0 << SCONV_pos)  | // 0: no 'single conversion' needed either, we are using timer 2 instead to trigger conversions at regular intervals
	          (0 << CS_pos);      // select channel 0 (AIN0)
}

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

	EADC = 1;  // Enable ADC interrupt for amplitude measure mode
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

void main(void) {
	uint8 prev_mode;

	initialDisplaySetup(); // set up the SPI display, including a display test long enough for humans to see all LEDs light up
	timer2setup();
	setup_heartbeat(); // Start the heartbeat LED blinking to show that the system is alive and running
	setup_interrupts_and_timers();

	//i = 0;
	//while(1) {
	//	i++;
	//	update_display_via_iir(i);
	//}

	// Configure switches for user input and begin reading their values
	SWITCHES = 0xFF;  // Make switch pins inputs
	T1 = 1; // put timer 1 counter input in input mode  TODO why is this in main??
	prev_mode = 0x8;

	while(1) {
		// Read switches and enter appropriate mode
		current_mode = MODE_SWITCHES;
		if (prev_mode != current_mode) {
			setup_interrupts_and_timers(); // Set interrupts and timers back to their default configurations
			reset_iir(); // Reset the IIR filter for the display when we switch modes (this will set a flag that causes it to overwrite the IIR value on the next update, then return to normal IIR operation)

			if (current_mode == DC_MODE) {
				// DC measurement mode
				spiWrite(MAX7219_DIGIT8_ADDR,				1); // TODO replace these SPI writes with using the LEDs on port 0
				setup_dc_measure();
			} else if (current_mode == FREQUENCY_MODE) {
				// Frequency measurement mode
				spiWrite(MAX7219_DIGIT8_ADDR,				2);
				setup_frequency_measure(); 
			} else if (current_mode == AMPLITUDE_MODE) {
				// Amplitude measurement mode
				spiWrite(MAX7219_DIGIT8_ADDR,				3);
				setup_amplitude_measure();
			} else if (current_mode == DISPLAY_TEST_MODE) {
				// Display test mode
				spiWrite(MAX7219_DIGIT8_ADDR,				4);
				//setup_display_test(); TODO
			}
		}
		prev_mode = current_mode;
	}
}  // end main
