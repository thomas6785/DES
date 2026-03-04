#define SWITCHES  	P2		// switches are connected to Port 2
#define SPI_LOAD 		WR

// TODO comments on all of these
#define TF2_pos 			(7)
#define EXF2_pos 			(6)
#define RCLK_pos 			(5)
#define TCLK_pos			(4)
#define EXEN2_pos 		(3)
#define TR2_pos				(2)
#define CNT2_pos 			(1)
#define CAP2_pos 			(0)

#define MD1_pos 			(7)
#define EXT_REF_pos 	(6)
#define CK1_pos 			(5)
#define CK0_pos				(4)
#define AQ_pos 				(2)
#define T2C_pos 			(1)
#define EXC_pos 			(0)

#define ADCI_pos 			(7)
#define DMA_pos 			(6)
#define CCONV_pos 		(5)
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

// Interrupt enable register bit positions
#define EA_pos					(7) // Global interrupt enable bit
#define EADC_pos				(6) // ADC interrupt enable bit
#define ET2_pos					(5) // Timer 2 interrupt enable bit
#define ES_pos					(4) // Serial interrupt enable bit
#define ET1_pos					(3) // Timer 1 interrupt enable bit
#define EX1_pos					(2) // External interrupt 1 enable bit
#define ET0_pos					(1) // Timer 0 interrupt enable bit
#define EX0_pos					(0) // External interrupt 0 enable bit

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

#define DC_MODE						(0)
#define FREQUENCY_MODE		(1)
#define AMPLITUDE_MODE		(2)
#define DISPLAY_TEST_MODE	(3)

#define HEARTBEAT_LED		T0

#define HEX_NOT_DEC_SWITCH		((SWITCHES & 0x80) >> 7) // bit 7 of SWITCHES controls whether we display in hex or decimal
#define IIR_SPEED_SWITCHES		((SWITCHES & 0x40) >> 6) // bit 6 of SWITCHES control the speed of the IIR filter
#define HEARTBEAT_SWITCH			((SWITCHES & 0x20) >> 5) // bit 5 of SWITCHES controls whether the heartbeat LED is on or off
#define SCALE_UNITS_SWITCHES	((SWITCHES & 0x10) >> 4) // bit 4 of SWITCHES control the units we display in (e.g. Hz vs kHz or mV vs V)

#define MODE_SWITCHES					(SWITCHES & 0x03) // bits 0 and 1 of SWITCHES control the mode of operation

void spiWrite(uint8 address, uint8 data_value);
void updateDisplay();
void reset_iir();
void initialDisplaySetup();

int16 get_dc_mode_measurement(void);
void setup_dc_measure(void);

void feed_iir(uint32 value_in);
