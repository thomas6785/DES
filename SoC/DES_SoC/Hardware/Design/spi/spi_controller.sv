`timescale 1ns/1ps

import spi_pkg::*;

module spi_controller (
    input clk,
    input rst_n,

    // Interface with the CSR registers
    input reg_access [1:0],
    input reg_write,
    input [31:0] reg_wdata,
    output [31:0] reg_rdata [1:0],
    output reg_error,
    output reg_ready,

    // Interface with the SPI core
    output trigger,
    output clk_div_e clk_div,
    output logic clk_pol,
    output tx_size_e tx_size,
    input core_busy,
    output [31:0] spi_data_in,
    input [31:0] spi_data_out,

    // IRQ
    output logic irq
);
    /*
    Control logic for the SPI core
    Implements the control/status register and data register
    As well as an FSM for busy/idle and an associated IRQ signal
    */

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Signal declarations
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // Control/status register
    logic control_reg_access;
    logic control_reg_load;
    logic        trigger_on_read;

    logic        busy;

    // Data register
    logic data_reg_access;

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Implement the control/status register
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    assign control_reg_access = reg_access[0]; // address 0
    assign control_reg_load = control_reg_access && reg_write && reg_ready;
    assign reg_rdata[0] = {
        irq,                // bit 31 - transaction complete interrupt
        busy,               // bit 30 - busy status
        23'b0,              // reserved
        clk_pol,            // bit 6 - clock polarity
        tx_size,            // bits 5:3 - transaction size (enum to a LUT of preset values)
        trigger_on_read,    // bit 2 - boolean whether to trigger the SPI FSM on an AHB data read (useful for streaming large amounts of data from the SPI slave)
        clk_div             // bits 1:0 - clock divide select (enum to a LUT of preset values)
    };

    always_ff @ (posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            clk_pol         <= '0;
            tx_size         <= TX_32_BIT;
            trigger_on_read <= '0;
            clk_div         <= CLK_DIV_8;
        end
        else if (control_reg_load)  begin
            clk_pol         <=            reg_wdata[6];
            tx_size         <= tx_size_e'(reg_wdata[5:3]);
            trigger_on_read <=            reg_wdata[2];
            clk_div         <= clk_div_e'(reg_wdata[1:0]);
        end
    end

    logic clear_irq;
    assign clear_irq = control_reg_load && reg_wdata[31]; // writing a 1 to bit 31 of the control register clears the IRQ

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Implement the data register
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    assign data_reg_access = reg_access[1]; // address 1
    assign reg_rdata[1] = spi_data_out; // read the received data from the SPI core

    // There is a flop inside SPI core so we don't need to flop tx data
    assign spi_data_in = reg_wdata;
    // does nothing unless there is a trigger (below)

    assign trigger = (data_reg_access && reg_ready && (reg_write || trigger_on_read)); // trigger on a write to the data register, or a read if 'trigger_on_read' is set

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // FSM
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    typedef enum logic [1:0] {
        IDLE,
        BUSY,
        DONE
    } state_e;
    state_e state, next_state;

    always_ff @ (posedge clk or negedge rst_n) begin
        if (!rst_n)     state <= IDLE;
        else            state <= next_state;
    end

    always_comb begin
        unique case (state)
            IDLE:   next_state = trigger   ? BUSY : IDLE;   // trigger moves us from IDLE to BUSY
            BUSY:   next_state = core_busy ? BUSY : DONE;   // stay in busy until 'core_busy' clears
            DONE:   next_state = clear_irq ? IDLE : DONE;   // return to IDLE when clear_irq comes
            default: next_state = IDLE; // shouldn't happen
        endcase
    end
    // TODO check if this is introducing a delay - is that problematic? I don't think so, just means the IRQ doesn't come until a cycle after 'core_busy' goes low, which should be fine

    assign busy = (state == BUSY);
    assign irq = (state == DONE);
    assign reg_ready = (state == IDLE) || (state == DONE) || (control_reg_access && !reg_write);
    assign reg_error = 0; // for now
    // registers are always ready to be read/written if we are idle/done
    // if a transaction is in flight, only allow reading the control/status
    // otherwise stall
    // Note that a transaction could be as long as 1000 clock cycles in the extreme case,
    // which would be a very long time to hang the AHB bus
    // software should take care to avoid this
    // We could reconfigure to give an error instead of stalling
endmodule
