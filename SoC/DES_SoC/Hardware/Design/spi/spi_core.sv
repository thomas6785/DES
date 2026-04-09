`timescale 1ns/1ps

import spi_pkg::*;

module spi_core #(
    parameter DWIDTH = 32,
    parameter CLK_DIV_BITS = 4
) (
    input clk,
    input rst_n,

    input                                   trigger,            // assume that trigger is a pulse and never coincides with 'busy'
    input        clk_div_e                  clk_div,            // number of clock cycles between SCLK edges
    input                                   clk_pol,            // clock polarity - 0: idle low, 1: idle high
    input        tx_size_e                  tx_size,            // mapped via LUT to the transaction size (one of 1,2,3,4,8,16,24,32 bits)
    output wire                             busy,

    input  wire  [DWIDTH-1:0]               data_in,            // data in ; MSB's will be ignored if tx_size is less than DWITCH
    output wire  [DWIDTH-1:0]               data_out,           // data out ; MSB's will be 0 if tx_size is less than DWIDTH. Only valid when 'busy' is low

	// SPI signals
	output wire                             MOSI,               // master out slave in serial data 
	input  wire                             MISO,               // master in slave out serial data
	output logic                            SCLK                // SPI clock (driven by master, clock stretching not supported TODO)
);
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Create a periodic pulse using a counter
    logic [CLK_DIV_BITS-1:0] clk_div_ctr;
    logic clk_div_ctr_null;
    assign clk_div_ctr_null = (clk_div_ctr == 0);

    logic [CLK_DIV_BITS-1:0] clk_div_reload_val;
    always_comb begin
        unique case (clk_div)
            CLK_DIV_4:   clk_div_reload_val =  1; // toggle SCLK every 2 cycles
            CLK_DIV_8:   clk_div_reload_val =  3; // toggle SCLK every 4 cycles
            CLK_DIV_16:  clk_div_reload_val =  7; // toggle SCLK every 8 cycles
            CLK_DIV_32:  clk_div_reload_val = 15; // toggle SCLK every 32 cycles
            default:     clk_div_reload_val = '0; // shouldn't happen
        endcase
    end

    always_ff @ (posedge clk or negedge rst_n) begin
        if (!rst_n)                 clk_div_ctr <= -1;                  // reset to -1 because 0 triggers other behaviour
        else if (trigger)           clk_div_ctr <= clk_div_reload_val;  // for a fresh transaction we should have a delay before the first SCLK edge to allow MOSI to stabilise
        else if (clk_div_ctr_null)  clk_div_ctr <= clk_div_reload_val;  // reload counter when it hits 0
        else if (busy)              clk_div_ctr <= clk_div_ctr - 1;     // count down when busy
        else                        clk_div_ctr <= clk_div_ctr;         // otherwise hold value
    end

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Use that periodic pulse to create SCLK
    logic sclk_int, sclk_next;

    assign sclk_next = sclk_int ^ clk_div_ctr_null; // toggle SCLK when counter hits 0
    always_ff @ (posedge clk or negedge rst_n) begin
        if (!rst_n) sclk_int <= 0;
        else        sclk_int <= sclk_next;
    end
    logic sclk_edge, sclk_leading_edge, sclk_trailing_edge;
    assign sclk_edge = sclk_next ^ sclk_int; // detect edges of SCLK (actually just equivalent to clk_div_ctr_null but we have written it explicitly for clarity)
    assign sclk_leading_edge  = sclk_edge & ~sclk_int;  // leading edge is when SCLK goes from 0 to 1
    assign sclk_trailing_edge = sclk_edge & sclk_int; // trailing edge is when SCLK goes from 1 to 0

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Decode the tx_size into the number of bits to transmit using a LUT
    // There a pre-defined values for the number of bit to transmit chosen to balance flexibility with logic cost
    logic [6-1:0] n_tx_bits_internal;

    always_comb begin
        unique case (tx_size)
            TX_01_BIT: n_tx_bits_internal = 1; // by providing 1, we can transmit any arbitrary bitwidth if necessary, with some software overhead
            TX_02_BIT: n_tx_bits_internal = 2;
            TX_03_BIT: n_tx_bits_internal = 3;
            TX_04_BIT: n_tx_bits_internal = 4;
            TX_08_BIT: n_tx_bits_internal = 8;
            TX_16_BIT: n_tx_bits_internal = 16;
            TX_24_BIT: n_tx_bits_internal = 24;
            TX_32_BIT: n_tx_bits_internal = 32;
        endcase
    end

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Create the counter for counting the number of bits in this transaction
    // Reloads to tx_size when the trigger comes
    // Then counts down on each trailing edge until it hits zero
    logic [$clog2(DWIDTH)+1-1:0] bit_ctr; // needs to be wide enough to hold the value DWIDTH, so one extra bit
    always_ff @ (posedge clk or negedge rst_n) begin
        if (!rst_n)                  bit_ctr <= '0;
        else if (trigger)            bit_ctr <= n_tx_bits_internal; // new transaction, reload the counter
        else if (sclk_trailing_edge) bit_ctr <= bit_ctr - 1; // count down on trailing edge so we reach zero on the final trailing edge
        else                         bit_ctr <= bit_ctr; // hold value otherwise
    end
    assign busy = (bit_ctr != 0); // busy whenever there are bits left to transmit

    
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Create the shift register (and a buffer flop) for transmitting and receiving data
    // Shift register updates on the trailing edge
    // The buffer flop samples MISO on the leading edge and feeds into the shift register
    // The buffer flop also doubles as a synchroniser flop for MISO to avoid metastability issues
    logic [DWIDTH-1:0] shift_reg;
    logic miso_buffer;

    always_ff @ (posedge clk or negedge rst_n) begin
        if (!rst_n)                  shift_reg <= '0;
        else if (trigger)            shift_reg <= data_in; // load data into shift register at the start of a transaction
        else if (sclk_trailing_edge) shift_reg <= {shift_reg[DWIDTH-2:0], miso_buffer}; // shift the shift register on the trailing edge, bringing in the new bit from MISO
        else                         shift_reg <= shift_reg; // hold value otherwise
    end

    always_ff @ (posedge clk or negedge rst_n) begin
        if (!rst_n)                 miso_buffer <= 0;
        else if (sclk_leading_edge) miso_buffer <= MISO; // sample MISO on the leading edge into the buffer flop
        else                        miso_buffer <= miso_buffer; // hold value otherwise
    end

    assign MOSI = shift_reg[n_tx_bits_internal-1]; // output the MSB of the shift register as MOSI. Note if our transaction is only n bits, we should send the nth bit from the shift register, ignoring the more significant bits
    logic [DWIDTH-1:0] data_out_mask;
    assign data_out_mask = (1<<n_tx_bits_internal) - 1;
    assign data_out = shift_reg & data_out_mask; // mask off the irrelevant upper bits of the shift register when outputting the received data

    assign SCLK = sclk_int ^ clk_pol; // if clock idles high, we can simply invert it here
endmodule

/*
SPI core overview:
    - one counter which creates a periodic pulse used for generating SCLK
    - SCLK toggles on that periodic pulse, and we also detect the edges of SCLK from that
    - one counter for counting the number of bits left to transmit in the current transaction (which is loaded at the start of a transaction, and counts the edges of SCLK until we are done)
    - a shared shift register for both transmitting and receiving

Note:
    - data_out is invalid when 'busy' is high, valid otherwise
    - 'trigger' should we a single-cycle pulse
    - 'trigger' should not go high while 'busy' is high

Invariants to check for:
    - MOSI should only change on the trailing edge of SCLK
    - MISO should be stable on the rising edge of SCLK
    - The internal state when 'busy' is low should be always identical to the internal state just after reset
        - This isn't quite true because clk_div_ctr gets reloaded to clk_div_reload_val at the end of a transaction
        - but this is irrelevant
    - No internal flops should change while busy and trigger are both low

Re. SPI timing:
- We sample MISO on the leading edge (And the slave should sample MOSI on the leading edge)
- and we update the shift register on the trailing edge (which affects MOSI)
- We first sample MISO into a buffer flop on the leading edge, then when the shift reg updates we feed that into the LSB
- This buffer flop gives a half-period of SCLK for any metastability to collapse (more than enough time)

Example waveform
https://wavedrom.com/editor.html?%7Bsignal%3A%20%5B%0A%20%20%7Bname%3A%20%27clk%27%2C%20%20%20%20%20%20wave%3A%20%27p...........................................%27%7D%2C%0A%20%20%7Bname%3A%20%27trigger%27%2C%20%20wave%3A%27010..........................................%27%7D%2C%0A%20%20%7Bname%3A%20%27data_in%27%2C%20%20wave%3A%20%27x%3Dx.........................................%27%2C%20data%3A%20%5B%27wxyz%27%5D%7D%2C%0A%20%20%7Bname%3A%20%27SCLK%27%2C%20%20%20%20%20wave%3A%20%27L.....H...L...H...L...H...L...H...L.........%27%7D%2C%0A%20%20%7Bname%3A%20%27MOSI%27%2C%20%20%20%20%20wave%3A%20%27xx%3D.......%3D.......%3D.......%3D.......x.........%27%2C%20data%3A%20%5B%27w%27%2C%27x%27%2C%27y%27%2C%27z%27%5D%7D%2C%0A%20%20%7Bname%3A%20%27MISO%27%2C%20%20%20%20%20wave%3A%20%27%3D..........%3D.......%3D.......%3D.......xxxxxxxxx%27%2C%20data%3A%20%5B%27a%27%2C%27b%27%2C%27c%27%2C%27d%27%5D%7D%2C%0A%20%20%7Bname%3A%20%27busy%27%2C%20%20%20%20%20wave%3A%20%270.1...............................0.........%27%7D%2C%0A%20%20%7Bname%3A%20%27shiftreg%27%2C%20wave%3A%20%27xx%3Dxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx%3D.........%27%2C%20data%3A%20%5B%27wxyz%27%2C%27abcd%27%5D%7D%0A%5D%7D%0A

For now, only supporting leading edge sampling
(change data on falling edge)

Trailing edge sampling could easily be added by simply swapping the logic for sampling miso_buffer and the shift register

*/