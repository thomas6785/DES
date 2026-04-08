typedef enum bit [2:0] {
    TX_01_BIT = 3'h0,
    TX_02_BIT = 3'h1,
    TX_03_BIT = 3'h2,
    TX_04_BIT = 3'h3,
    TX_08_BIT = 3'h4,
    TX_16_BIT = 3'h5,
    TX_24_BIT = 3'h6,
    TX_32_BIT = 3'h7
} tx_size_e;
// TODO move enum to appropriate package or header or something

module AHBspi #(
    parameter DWIDTH = 32,
    parameter CLK_DIV_BITS = 4
) (
    input HCLK,
    input HRESETn,
    input trigger,
    input [$clog2(CLK_DIV_BITS)-1:0]    decrement_amt,  // position of the bit to decrement in the counter
                                                        // 0 will give the maximum clock period (counter will count down by 1<<0=1 at a time)
                                                        // 3 will give a divide by 16 (counter will count down by 1<<3=8 at a time)
    input tx_size_e            tx_size,                     // mapped via LUT to the transaction size (one of 1,2,3,4,8,16,24,32 bits)
    output wire busy,

    input  wire [DWIDTH-1:0]                  data_in,
    output wire [DWIDTH-1:0]                  data_out,

	// SPI signals
	output wire  MOSI,		// master out slave in serial data 
	input  wire  MISO,		// master in slave out serial data
	output logic SCLK		// SPI clock (driven by master, clock stretching not supported TODO)
);
    // Basic operating principle:
    //      A large counter register 'ctr' is created
    //      The 4 LSBs are used as a divider to generate SCLK
    //      So ctr[3] is used as SCLK
    //      The remaining MSBs count the number of bits to be transmitted
    //      When a transaction is triggered, the MSBs are loaded with the number of bits being transmitted
    //      Then we decrement either ctr[0], ctr[1], ctr[2], or ctr[3] on each cycle (adjusting this let's us adjust the SCLK frequency)
    //      This will cause SCLK to toggle at a divided rate
    //      and each time SCLK toggles, the counter counting the number of bits will decrement
    //      There is also a shift register for MISO and MOSI,
    //      and logic for detecting edges in SCLK so we know when to shift the shift register

    logic [6+CLK_DIV_BITS-1:0] ctr;

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

    // Main counter logic
    // Reload value on trigger, and decrement otherwise
    // Stopping at zero
    always_ff @ (posedge HCLK) begin
        if (!HRESETn)       ctr <= '0;
        else if (trigger)   ctr <= {n_tx_bits_internal,{CLK_DIV_BITS{1'b0}}}; // load the counter with the number of bits to transmit, left-shifted to give space for the clock divide bits
        else if (ctr != 0)  ctr <= ctr - (1<<decrement_amt);             // decrement
        else                ctr <= ctr;
    end
    // decrement_amt must remain stable during a transaction because it is used directly here

    assign busy = (ctr != 0);

    // Assign SCLK to ctr[CLK_DIV_BITS-1]
    // route it through a flop to remove any glitches from the counter logic
    // this also makes it easier to detect edges
    logic sclk_next;
    assign sclk_next = ctr[CLK_DIV_BITS-1];
    always_ff @ (posedge HCLK) begin
        if (!HRESETn) SCLK <= 0;
        else SCLK <= sclk_next;
    end
    logic sclk_edge,sclk_rising_edge, sclk_falling_edge;
    assign sclk_edge = sclk_next != SCLK;
    assign sclk_rising_edge = sclk_edge && !SCLK; // rising edge when sclk is low and about to go high
    assign sclk_falling_edge = sclk_edge && SCLK; // falling edge when sclk is high and about to go low

    // LSBs of 'ctr' should always reload to zero - they are there to divide SCLK down
    // Change the decrement to change the divide period - must always be a power of 2 to ensure 50/50 duty cycle

    logic [DWIDTH-1:0] shift_register;
    always_ff @ (posedge HCLK) begin
        if (!HRESETn) shift_register <= '0;
        else if (trigger) shift_register <= data_in; // load data to be transmitted
        else if (sclk_rising_edge) shift_register <= {shift_register[DWIDTH-2:0], MISO}; // shift in on SCLK rising edge
    end
    // TODO we should support the falling edge instead of rising edge
    // TOOD we should support configurable clock polarity (idle low vs idle high)
    //          TODO simplest way to do this is simply XOR SCLK with CPOL

    assign MOSI = shift_register[n_tx_bits_internal-1]; // output the MSB first
                                                        // note that this MUX is not as expensive as it looks because n_tx_bits_internal can only take 8 possible values, so this is an 8:1 MUX not 32:1
    assign data_out = shift_register; // output the current state of the shift register
endmodule

module AHBspi_TB;
    logic clk;
    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end

    logic MOSI,MISO,SCLK;
    logic HRESETn, trigger;
    logic [1:0] decrement_amt;
    tx_size_e tx_size;
    logic busy;
    logic [31:0] data_in, data_out;

    AHBspi #(
        .CLK_DIV_BITS(4)
    ) dut (
        .HCLK(clk),
        .HRESETn,
        .trigger,
        .decrement_amt,
        .tx_size,
        .busy,

        .data_in,
        .data_out,

    	.MOSI,
	    .MISO,
	    .SCLK
    );

    assign MISO = MOSI; // loopback for testing

    initial begin
        // Define all signals
        HRESETn = 0; trigger = 0; decrement_amt = 0;;
        repeat(2) @(posedge clk); // allow the reset to take effect
        HRESETn = 1; // release reset
        repeat(5) @(posedge clk); // do nothing for a moment
        $display("%5t Setup completed", $time);
        
        simple_test($urandom,0,TX_08_BIT);
        simple_test($urandom,3,TX_04_BIT);
        simple_test($urandom,2,TX_16_BIT);
        simple_test($urandom,1,TX_32_BIT);

        force dut.MISO = 1;
        simple_test($urandom,0,TX_32_BIT);
        assert (data_out == 32'hFFFF_FFFF) else $error("Expected to receive all 1s when forcing MISO high, but received %8h", data_out);
        release dut.MISO;

        force dut.MISO = 0;
        simple_test(32'hDEADBEEF,0,TX_16_BIT);
        assert (data_out[15:0] == 16'b0) else $error("Expected to receive all 0s in the lower 16 bits when forcing MISO low, but received %8h", data_out);

        simple_test(32'hCAFECAFE,0,TX_01_BIT);
        simple_test(32'hCAFECAFE,0,TX_02_BIT);
        simple_test(32'hCAFECAFE,0,TX_03_BIT);
        simple_test(32'hCAFECAFE,0,TX_04_BIT);
        simple_test(32'hCAFECAFE,0,TX_08_BIT);
        simple_test(32'hCAFECAFE,0,TX_16_BIT);
        simple_test(32'hCAFECAFE,0,TX_24_BIT);
        simple_test(32'hCAFECAFE,0,TX_32_BIT);
        
        release dut.MISO;

        $stop;
    end

    task simple_test(logic [31:0] _wdata, int _decrement_amt, tx_size_e _tx_size);
        $display("%5t Starting simple_test with decrement_amt=%0d, tx_size=%s, wdata=%8h", $time, _decrement_amt, _tx_size.name(), _wdata);
        decrement_amt = _decrement_amt;
        tx_size = _tx_size;
        data_in = _wdata;
        trigger = 1;
        @(posedge clk); trigger = 0;
        @(negedge busy); repeat(5) @(posedge clk);
        $display("%5t Wrote %8h, received %8h", $time, _wdata, data_out);
    endtask
endmodule

/*

Some notes on real-world effects: timing and instability

Re. incoming metastability on MISO:
- The incoming MISO signal needs synchronisation to avoid metastability
- The shift register can effectively act as this synchroniser
- However we should wait 1-2 cycles after completing a transaction to ensure the last bit in the shift register is not metastable
- This condition is already provided because 'busy' doesn't go low until 'ctr' is empty
    - unless we are decrementing at the max speed
    - so we need to delay the 'busy' signal

MOSI:
- The slave will sample MOSI "on the rising edge of SCLK" (or falling edge, depending on the SPI mode)
- This means that MOSI must be stable before the rising edge
    - Our STA will cover this fine because our system clock is significantly faster than SCLK anyway
- It also means MOSI must be stable AFTER the rising edge
    - This is more difficult
    - The sum:
        - clock-to-Q pin time in MSB of shift reg
        - propagation delay from the MSB shift reg to MOSI
        - propagation delay along MOSI into the slave
      must be less than the slave's hold time

    So the MOSI change has to happen somewhere inbetween consecutive edges of SCLK
    The exact optimum position depends on the propagation delay (which is unknown) and the difference between the slave's setup and hold times (which is also unknown)
    However it's reasonable to assume that putting it halfway will be more than enough margin, and this makes the logic simpler for our bridge
    So we need MOSI to change halfway between rising edges of SCLK i.e. on falling edges of SCLK

*/
