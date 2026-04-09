`timescale 1ns/1ps

module AHBspi (
	// AHB bus signals
	input  wire        HCLK,	// bus clock
	input  wire        HRESETn,	// bus reset, active low
	input  wire        HSEL,	// selects this slave
	input  wire        HREADY,	// indicates previous transaction completing
	input  wire [31:0] HADDR,	// address
	input  wire [1:0]  HTRANS,	// transaction type (only bit 1 used)
	input  wire        HWRITE,	// write transaction
	input  wire [31:0] HWDATA,	// write data
	output wire [31:0] HRDATA,	// read data from slave
	output wire        HREADYOUT,	// ready output from slave
    output wire        HRESP,	// response output from slave

	// SPI signals
	output wire MOSI,		// master out slave in serial data 
	input  wire MISO,		// master in slave out serial data
	output wire SCLK,		// SPI clock (driven by master, clock stretching not supported TODO)

    // IRQ
    output wire IRQ         // IRQ is sticky, indicates end of transaction. Write the status register to clear it.
);
    /*
    SPI bridge is broken into three components:
        - AHB_CSR module:
            de-pipelines AHB to give a simple memory interface for CSR registers
        - SPI controller:
            Implements the control/status register and data register
            As well as an FSM for busy/idle and an associated IRQ signal
        - SPI core:
            Implements the actual SPI transaction logic
            Shift register for MISO/MOSI
            Counter of number of bits transmitted to know when transaction is done
            clock divider
    
    There are two registers (control/status and data)
    Bitfields are shown in the documentation
    
    CAUTION: Writing to a register while a transaction is in progress will cause the AHB bus
             to stall until that transaction completes. For a slow clock and a long transaction,
             this could be as much as 1024 clock cycles.

    Does NOT provide a chip-select signal
    This must be handled externally e.g. with GPIO
    */

    // Interface between SPI controller and AHB CSR module
    logic        reg_access [1:0];
    logic [31:0] reg_rdata  [1:0];
    logic        reg_write;
    logic [31:0] reg_wdata;
    logic        reg_error;
    logic        reg_ready;

    // Interface between SPI controller and SPI core
    logic        trigger;
    clk_div_e    clk_div;
    logic        clk_pol;
    tx_size_e    tx_size;
    logic        core_busy;
    logic [31:0] spi_data_in;
    logic [31:0] spi_data_out;

    // SPI Controller
    spi_controller I_spi_controller (
        .clk(HCLK),
        .rst_n(HRESETn),

        .reg_access,
        .reg_write,
        .reg_wdata,
        .reg_rdata,
        .reg_error,
        .reg_ready,
        
        .trigger,
        .clk_div,
        .clk_pol,
        .tx_size,
        .core_busy,
        .spi_data_in,
        .spi_data_out,

        .irq(IRQ)
    );

    // Map the AHB interface to the simpler CSR interface
    ahb_to_csr #(
        .ADDR_WIDTH(10),
        .NUM_REGS(2) // control/status and data
    ) csr_regs (
        .HCLK, .HRESETn, .HSEL, .HREADY, .HTRANS, .HWRITE, .HWDATA, .HRDATA, .HREADYOUT, .HRESP,
        .HADDR(HADDR[9:0]), // 10 bit address space (minimum allowed by AHB)

        .access(reg_access),
        .write(reg_write),
        .wdata(reg_wdata),
        .rdata(reg_rdata),
        .error(reg_error),
        .ready(reg_ready)
    );

    // SPI core
    spi_core I_spi_core (
        .clk(HCLK),
        .rst_n(HRESETn),

        .trigger,
        .clk_div,
        .clk_pol,
        .tx_size,
        .busy(core_busy),
        .data_in(spi_data_in),
        .data_out(spi_data_out),
        
        .MOSI,
        .MISO,
        .SCLK
    );
endmodule