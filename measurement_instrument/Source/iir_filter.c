#include <ADUC841.H>	// special function register definitions
#include "typedef.h"	// variable type definitions
#include "main.h"

static uint8 reset_iir_on_next_input = 0; // Flag to indicate if we should reset the IIR filter to the NEXT input value. This is set to 1 when we switch modes and then reset to 0 after the first update. This is necessary because otherwise when we switch modes the IIR filter will still be at the value from the previous mode and it will take a long time to converge to the new value, which is confusing for users.
static uint32 iir_value_full = 0; // IIR filter for display value. Initialise to zero. Static so it is only available inside this file
uint16 iir_value = 0; // Not static because it is read by main to update the display

void feed_iir(uint32 value_in) { // TODO should use camelCase not snake_case
	// Wrapper function which updates an IIR filter and then calls display_value to update the display.

	// Even though the display value is only 16 bits, we will use a 32-bit variable for the IIR filter for two reasons:
	// 1) to avoid overflow when multiplying by forgetting factor
	// 2) to allow for more precision in the IIR filter, which will make it converge faster to the new value when we switch modes (if we only use a 16-bit variable then we will lose precision in the IIR filter which will make it take longer to converge to the new value when we switch modes, which is confusing for users)

	// The IIR filter has 32-bits
	// The display value is the middle 16 bits
	// giving 8 LSBs of hidden precision

	// Update IIR filter
	if (reset_iir_on_next_input) {
		// If reset_iir_on_next_input is 1, reset the IIR filter to the new value immediately
		iir_value_full = value_in << 8; // Shift the value to the middle 16 bits
		reset_iir_on_next_input = 0; // Reset the flag
	} else {
		// If reset_iir_on_next_input is 0, update the IIR filter with the new value using a simple IIR filter with a time constant of 8 samples
		if (IIR_SPEED_SWITCHES)				iir_value_full = ((7*value_in) << 5) + (iir_value_full >> 3);
		else													iir_value_full = ((15*value_in) << 4) + (iir_value_full >> 4);

		// we only left shift the new value by 5 because we want to left shift by 8 bits then multiply by 7/8
		// right-shift the old value by 3 to divide by 8
		// TODO make the forgetting factor configurable by the user
		// TODO this forgetting factor is all wrong
		// TODO forgetting factor should be much smaller in DC mode since it receives a new value at the rate of CCONV
		// TODO forgetting factor also needs to be user configurable
	}

	iir_value = iir_value_full >> 8; // Use the middle 16 bits of the 32-bit IIR filter output
}
// TODO this IIR filter is not well-implemented - we should be retaining more than 1/8th of the old value
// TODO the problem is that it will be too slow if we only update every 10/64 seconds, so refactor to call this much more frequently
// TODO this will affect scaling/shifting logic a bit
// TODO skip SPI writes if the value is unchanged

void reset_iir() {
	reset_iir_on_next_input = 1;
}
