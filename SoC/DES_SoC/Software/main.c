#include <stdio.h>					// needed for printf
#include "DES_M0_SoC.h"			// defines registers in the hardware blocks used
#define FLASH_DELAY					1000000		// delay for flashing LEDs, ~220 ms

typedef struct {
    int16 x;
    int16 y;
    int16 z;
} coords;

volatile coords measured_coords; // global variable to store the most recent accelerometer measurements, updated by the SysTick ISR and read by the main loop
volatile int16 temp_value;
volatile uint8 fresh_data_flag = 0; // flag to indicate that new data has been sampled by the SysTick interrupt and is ready for processing

void UART_ISR() {} // UART ISR not used, but expected by the linker

void delay (uint32 n) {
    volatile uint32 i;
    for(i=0; i<n; i++);             // do nothing n times
}

coords filter_coords(coords raw) {
    static coords coords_iir;

    coords_iir.x = (7*coords_iir.x + raw.x) >> 3;
    coords_iir.y = (7*coords_iir.y + raw.y) >> 3;
    coords_iir.z = (7*coords_iir.z + raw.z) >> 3;

    return coords_iir;
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
	//read_val = pt2SPIDAT; // TODO REMOVE THIS we shouldn't be reading back so quickly but it's usefulf or debugging
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
	write_accelerometer_register(0x1F,0x52); // 0x52 is the instruction to reset ('R' in ASCII)
    delay(FLASH_DELAY); // TODO reduce flash delay
	write_accelerometer_register(0x2d, 0x2);
}

coords read_accelerometer() {
    coords c;

    c.x = (read_accelerometer_register(0x0F) << 8) | read_accelerometer_register(0x0E); // read the x value (high byte first)
    c.y = (read_accelerometer_register(0x11) << 8) | read_accelerometer_register(0x10);
    c.z = (read_accelerometer_register(0x13) << 8) | read_accelerometer_register(0x12);

    return c;
}

//////////////////////////////////////////////////////////////////
// Interrupt service routine for System Tick interrupt
//////////////////////////////////////////////////////////////////
void SysTick_ISR()
{
    int16 temp_l;
    int16 temp_h;

    //printf(".\n");

	// Sample the accelorometer and store the value of the data in global variables
    // Read the x, y and z values. Note that these are only 8 bits each.
    measured_coords = read_accelerometer(); // this function reads the 16 bit values for x, y and z, but we will only be using the lower 8 bits for this lab, as the upper 8 bits are not currently being updated by the hardware

    temp_l = read_accelerometer_register(0x14) ; // read the low byte of the temperature value
    temp_h = read_accelerometer_register(0x15) ; // read the high byte of the temperature value
    //ignore the 4 MSBs of the high byte, and combine with the low byte to get the 12 bit temperature value
    temp_value = ((temp_h) << 8) | temp_l;

    fresh_data_flag = 1; // set the flag to indicate that new data is ready for processing
}

void LED_from_middle(int8 value) {
    uint8 i;

    GPIO_LED = 0; // clear all LEDs

    // This funciton should light up the LEDs from the middle, with positive values lighting to the right
    // and negative values lighting to the left. The magnitude of the value determines how many LEDs are lit, in a one hot code (where the lights stay on)
    if (value > 0) {
        for (i = 0; i < value; i++) {
            GPIO_LED |= (1 << (7 + i)); // light up LEDs to the right of the middle
        }
    } else if (value < 0) {
        for (i = 0; i < -value; i++) {
            GPIO_LED |= (1 << (7 - i)); // light up LEDs to the left of the middle
        }
    } else {
        GPIO_LED = (1 << 7); // light up the middle LED
    }
}

void write_lower_half_to_display(int16 value) {
    // Write the value to the display, by writing to the pointers. This doesn't need to worry about encoding, as this is handled by the block
    pt2DISP->digit0 = value & 0xF; // write the lower nibble of the value to the first digit of the display
    pt2DISP->digit1 = (value >> 4) & 0xF;
    pt2DISP->digit2 = (value >> 8) & 0xF;
    pt2DISP->digit3 = (value >> 12) & 0xF;
}

void write_upper_half_to_display(int16 value) {
    // Write the value to the display, by writing to the pointers. This doesn't need to worry about encoding, as this is handled by the block
    pt2DISP->digit4 = value & 0xF; // write the lower nibble of the value to the third digit of the display
    pt2DISP->digit5 = (value >> 4) & 0xF;
    pt2DISP->digit6 = (value >> 8) & 0xF;
    pt2DISP->digit7 = (value >> 12) & 0xF;
    // TODO accommodate signs
}

coords get_calibrated_offsets() {
    coords offsets;
    uint8 i;

    offsets.x = 0;
    offsets.y = 0;
    offsets.z = 0;

    for(i=0;i<64;i++) { // take 8 samples and average them to get the offsets
        coords sample = read_accelerometer();
        offsets.x += sample.x;
        offsets.y += sample.y;
        offsets.z += sample.z;
        delay(100000); // add a delay between samples to avoid issues with sampling too quickly TODO tune this delay and all others
    }
    offsets.x = offsets.x >> 6;
    offsets.y = offsets.y >> 6;
    offsets.z = offsets.z >> 6;

    return offsets;
}

int main(void) {
    uint16 sw; // variable to store the value of the switches
    uint8 mode; // variable to store the mode of operation (tilt or position)

    int32 x_pos = 0;
    int32 y_pos = 0;
    int32 z_pos = 0;
    int32 x_vel = 0;
    int32 y_vel = 0;
    int32 z_vel = 0;

    coords cal_offsets;

    uint8 i;
    i=0;

    NVIC_Disable = 0xFFFFFFFF; // disable all interrupts
    NVIC_Enable = (1<<15);

    // Set systick to give ISR every 50 Hz
    SysTick_Reload   = 1000000; // set the reload value for the SysTick timer to generate an interrupt at 50 Hz (assuming a 50MHz clock)
    SysTick_Control  = 0x7;

    // Firstly, do all the configuring of the device here
    pt2DISPMODE     = 0xFF;
    pt2DIGITENABLE  = 0xFF;
    pt2SPICON       = (0 << 6) | (6 << 3) | (0 << 2) | (3);
    configure_accelerometer();

    delay(FLASH_DELAY); // TODO tighten this delay
    cal_offsets = get_calibrated_offsets();
    printf("Calibration offsets - x: %d, y: %d, z: %d\n",cal_offsets.x,cal_offsets.y,cal_offsets.z);

    for(i=0;i<20;i++) {
        filter_coords(cal_offsets); // initialize the filter state
    }

    while(1) {
        // Read the state of the switches
        sw = GPIO_SW;
        mode = sw >> 15; //Mode can either be in tilt mode or position mode, determined by the MSB of the switch value
        printf("Switch value: %4x, Mode: %d\n",sw,mode);

        if (mode == 0) { // This is tilt mode
            while(mode ==0) { // stay in this mode until the mode switch is toggled
                if (fresh_data_flag) { // check if new data is ready for processing
                    fresh_data_flag = 0; // reset the flag

                    // In this mode, we show the tilt around the y axis on the LEDs, and the tilt on the x axis numerically on the display
                    // The y tilt is a value between 0 and 255

                    // Display the y tilt on the LEDs
                    LED_from_middle(measured_coords.y>>8); // shift the value down to fit in the 8 LEDs, and display from the middle
                    // Display the x tilt directly on the 7 segment display
                    write_lower_half_to_display(measured_coords.x);
                }
                sw = GPIO_SW; // read the switches again to check if we need to change mode
                mode = sw >> 15;
            }
        } else {
            while(mode == 1) { // This is position mode, stay in this mode until the mode switch is toggled
                if (fresh_data_flag) { // check if new data is ready for processing
                    coords new_filters_coords;
                    fresh_data_flag = 0; // reset the flag

                    new_filters_coords = filter_coords(measured_coords);
                    printf("x_value: %d, y_value: %d, z_value: %d\n",new_filters_coords.x,new_filters_coords.y,new_filters_coords.z);

                    // In this mode, the user can select the measurement for the LEDs and both halves of the display using the switches.
                    // The value should be double integrated to get position
                    x_vel += new_filters_coords.x - cal_offsets.x;
                    y_vel += new_filters_coords.y - cal_offsets.y;
                    z_vel += new_filters_coords.z - cal_offsets.z;
                    printf("x_vel: %d, y_vel: %d, z_vel: %d\n",x_vel,y_vel,z_vel);

                    x_pos += x_vel; // integrate velocity to get position
                    y_pos += y_vel;
                    z_pos += z_vel;
                    printf("x_pos: %d, y_pos: %d, z_pos: %d\n",x_pos,y_pos,z_pos);
                    // Display the selected value on the LEDs and display
                    // For now just display the x to the LEDs, y to the lower half of the display and z to the upper half of the display, but this could be changed to allow the user to select which value goes where using the switches
                    LED_from_middle((new_filters_coords.x - cal_offsets.x)>>12); // shift the value down to fit in the 8 LEDs, and display from the middle
                    write_lower_half_to_display(new_filters_coords.y - cal_offsets.y); // right 4 digits
                    write_upper_half_to_display(new_filters_coords.z - cal_offsets.z); // left 4 digits
                }
                sw = GPIO_SW; // read the switches again to check if we need to change mode
                mode = sw >> 15;
                //delay(1000); // add a delay to avoid bouncing issues with the switch
                i = i+1;
                if (i==100) { // recalibrate every 100 loops
                    cal_offsets = get_calibrated_offsets();
                    x_vel = 0;
                    y_vel = 0;
                    z_vel = 0;
                }
            }
        }
    }
}
