module spi_core_TB;
    logic clk;
    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end

    logic MOSI,MISO,SCLK;
    logic rst_n, trigger;
    logic [3:0] clk_div_reload_val;
    tx_size_e tx_size;
    logic busy;
    logic [31:0] data_in, data_out;

    spi_core #(
        .CLK_DIV_BITS(4)
    ) dut (
        .clk,
        .rst_n,
        .trigger,
        .clk_div_reload_val,
        .tx_size,
        .clk_pol(1'b0),
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
        rst_n <= 0; trigger <= 0; clk_div_reload_val <= 8;
        repeat(2) @(posedge clk); // allow the reset to take effect
        rst_n <= 1; // release reset
        repeat(5) @(posedge clk); // do nothing for a moment
        $display("%5t Setup completed", $time);
        
        simple_test($urandom,15,TX_08_BIT);
        simple_test($urandom,3,TX_04_BIT);
        simple_test($urandom,2,TX_16_BIT);
        simple_test($urandom,1,TX_32_BIT);

        force dut.MISO = 1; $display("Forcing MISO=1");
        simple_test($urandom,5,TX_32_BIT);
        assert (data_out == 32'hFFFF_FFFF) else $error("Expected to receive all 1s when forcing MISO high, but received %8h", data_out);
        release dut.MISO;

        force dut.MISO = 0; $display("Forcing MISO=0");
        simple_test(32'hDEADBEEF,1,TX_16_BIT);
        assert (data_out[15:0] == 16'b0) else $error("Expected to receive all 0s in the lower 16 bits when forcing MISO low, but received %8h", data_out);
        release dut.MISO;

        simple_test(32'hCAFECAFE,8,TX_01_BIT);
        simple_test(32'hCAFECAFE,8,TX_02_BIT);
        simple_test(32'hCAFECAFE,8,TX_03_BIT);
        simple_test(32'hCAFECAFE,8,TX_04_BIT);
        simple_test(32'hCAFECAFE,8,TX_08_BIT);
        simple_test(32'hCAFECAFE,8,TX_16_BIT);
        simple_test(32'hCAFECAFE,8,TX_24_BIT);
        simple_test(32'hCAFECAFE,8,TX_32_BIT);
        
        release dut.MISO;

        // Try back-to-back transactions
        data_in <= 32'h12345678;
        trigger <= 1; @(posedge clk); trigger <= 0; // first transaction
        @(negedge busy); // wait for it to finish
        data_in <= 32'hDEADBEEF;
        trigger <= 1; @(posedge clk); trigger <= 0; // second transaction immediately
        @(negedge busy); @(posedge clk); // wait for it to finish
        $display("%5t Wrote %8h, received %8h", $time, data_in, data_out);

        repeat(10) @(posedge clk);
        
        $stop;
    end

    task simple_test(logic [31:0] _wdata, int _clk_div_reload_val, tx_size_e _tx_size);
        $display("%5t Starting simple_test with clk_div_reload_val=%0d, tx_size=%s, wdata=%8h", $time, _clk_div_reload_val, _tx_size.name(), _wdata);
        clk_div_reload_val <= _clk_div_reload_val;
        tx_size <= _tx_size;
        data_in <= _wdata;
        trigger <= 1;
        @(posedge clk); trigger <= 0;
        @(negedge busy); repeat(5) @(posedge clk);
        $display("%5t Wrote %8h, received %8h", $time, _wdata, data_out);
    endtask
endmodule

`define AHB_SPI_CONFIG_WRITE(data) HTRANS <= 2'b10; HWRITE <= 1; HADDR <= '0;   @(posedge clk iff HREADYOUT); HWDATA <= data;
`define AHB_SPI_CONFIG_READ        HTRANS <= 2'b10; HWRITE <= 0; HADDR <= '0;   @(posedge clk iff HREADYOUT);
`define AHB_SPI_DATA_WRITE(data)   HTRANS <= 2'b10; HWRITE <= 1; HADDR <= 1<<2; @(posedge clk iff HREADYOUT); HWDATA <= data;
`define AHB_SPI_DATA_READ          HTRANS <= 2'b10; HWRITE <= 0; HADDR <= 1<<2; @(posedge clk iff HREADYOUT);
`define AHB_SPI_IDLE               HTRANS <= 2'b00;

module AHBspi_TB;
    logic clk;
    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end

    logic MOSI,MISO,SCLK;

    logic rst_n;
    logic ready_out;

    logic [31:0] HADDR;
    logic  [1:0] HTRANS;
    logic        HWRITE;
    logic [31:0] HWDATA;
    logic [31:0] HRDATA;
    logic        HREADYOUT;
    logic        HRESP;

    logic IRQ;

    logic [31:0] slave_shift_register;

    always @ (posedge SCLK or negedge rst_n) begin
        if (!rst_n) begin
            slave_shift_register <= '0;
        end else begin
            slave_shift_register <= {slave_shift_register[30:0], MOSI};
        end
    end

    always_ff @ (negedge SCLK or negedge rst_n) begin
        if (!rst_n) begin
            MISO <= 0;
        end else begin
            MISO <= slave_shift_register[31];
        end
    end

    AHBspi dut (
        .HCLK(clk),
        .HRESETn(rst_n),
        .HSEL(1'b1),
        .HREADY(HREADYOUT),
        .HADDR,
        .HTRANS,
        .HWRITE,
        .HWDATA,
        .HRDATA,
        .HREADYOUT,
        .HRESP,

        .MISO,
        .MOSI,
        .SCLK,
        .IRQ
    );

    initial begin
        HWDATA <= '0; HTRANS <= '0; HWRITE <= 0; HADDR <= '0;
        rst_n <= 0; repeat(2) @(posedge clk); rst_n <= 1; // reset pulse

        // first try writing DEADBEEF
        $display("%4t Simple write test 1",$time);
        HTRANS <= 2'b10; // NONSEQ
        HWRITE <= 1;
        HADDR <= '0;
        @(posedge clk iff HREADYOUT);
        HWDATA <= 32'(TX_32_BIT<<3 | CLK_DIV_32);

        HADDR <= 1 << 2;
        @(posedge clk iff HREADYOUT);
        HWDATA <= 32'hDEADBEEF;

        @(posedge clk iff HREADYOUT);
        HTRANS <= 2'b00; // IDLE
        HWDATA <= 32'hCAFEBABE; // should be ignored

        @(posedge IRQ);
        assert (slave_shift_register == 32'hDEADBEEF) else $error("%4t Expected to receive DEADBEEF, but received %8h", $time, slave_shift_register);

        repeat(10) @(posedge clk);

        // try sending just 4 bits
        $display("%4t Simple write test 2",$time);
        `AHB_SPI_CONFIG_WRITE((1<<31) | TX_04_BIT<<3 | CLK_DIV_16)
        `AHB_SPI_DATA_WRITE(32'h5a5a5a5a)
        `AHB_SPI_IDLE
        @(posedge IRQ);
        assert (slave_shift_register[3:0] == 4'ha) else $error("%4t Expected to receive a in the top 4 bits, but received %8h", $time, slave_shift_register);

        // try sending only 1 bit
        $display("%4t Simple write test 3",$time);
        `AHB_SPI_CONFIG_WRITE((1<<31) | TX_01_BIT<<3 | CLK_DIV_4)
        `AHB_SPI_DATA_WRITE(32'hffff_ffff)
        `AHB_SPI_IDLE
        @(posedge IRQ);
        assert (slave_shift_register[0] == 1'b1) else $error("%4t Expected to receive 1 in the LSB, but received %8h", $time, slave_shift_register);

        // try sending 8 bits
        $display("%4t Simple write test 4",$time);
        `AHB_SPI_CONFIG_WRITE((1<<31) | TX_08_BIT<<3 | CLK_DIV_8)
        `AHB_SPI_DATA_WRITE(32'h12345678)
        `AHB_SPI_IDLE
        @(posedge IRQ);
        assert (slave_shift_register[7:0] == 8'h78) else $error("%4t Expected to receive 0x78 in the lower 8 bits, but received %8h", $time, slave_shift_register);

        @(posedge clk);
        assert (IRQ == 1) else $error("%4t IRQ unexpectedly low", $time, slave_shift_register);

        // Test clearing IRQ and changing config
        $display("%4t Clearing IRQ",$time);
        `AHB_SPI_CONFIG_WRITE((1<<31) | TX_32_BIT<<3 | CLK_DIV_8) // also test clearing the IRQ by writing a 1 to bit 31
        `AHB_SPI_IDLE
        @(posedge clk);
        assert (IRQ == 0) else $error("%4t Expected IRQ to be cleared after writing to control register with bit 31 set, but it was still high", $time, slave_shift_register);
        `AHB_SPI_CONFIG_WRITE((1<<31) | TX_03_BIT<<3 | CLK_DIV_4) // also test clearing the IRQ by writing a 1 to bit 31
        `AHB_SPI_IDLE
        @(posedge clk);
        assert (IRQ == 0) else $error("%4t Expected IRQ to be cleared after writing to control register with bit 31 set, but it was still high", $time, slave_shift_register);
        assert (slave_shift_register[7:0] == 8'h78) else $error("%4t Expected unchanged slave shift register but found %8h", $time, slave_shift_register);

        // attempt a read transaction without trigger on read
        $display("%4t Read test 1",$time);
        `AHB_SPI_CONFIG_WRITE((1<<31) | TX_32_BIT<<3 | CLK_DIV_8) // make sure trigger on read is disabled
        slave_shift_register <= 32'hA5A5A5A5; // preload the slave shift register with known data before the read
        `AHB_SPI_DATA_WRITE(32'habcdef01)
        `AHB_SPI_IDLE
        @(posedge IRQ); // wait for the transaction to complete
        `AHB_SPI_DATA_READ
        `AHB_SPI_IDLE
        assert (HRDATA == 32'hA5A5A5A5);
        assert (slave_shift_register == 32'habcdef01) else $error("%4t Expected slave shift register to be A5A5A5A5, but found %8h", $time, slave_shift_register);

        // attempt a read transaction WITH trigger on read
        $display("%4t Read test 2",$time);
        `AHB_SPI_CONFIG_WRITE((1<<31) | (1<<2) | TX_32_BIT<<3 | CLK_DIV_8) // enable trigger on read
        `AHB_SPI_DATA_READ
        `AHB_SPI_IDLE
        assert (HRDATA == 32'habcdef01); // the read data should be the same as the last written data, not the SPI data, since the trigger happens at the same time as the read
        @(posedge IRQ); // that should have auto-triggered a transaction
        assert (HRDATA == 32'hA5A5A5A5); // so now we should see the new data from the slave

        // back-to-back transactions should be fine (and stall)
        $display("%4t Back-to-back test",$time);
        `AHB_SPI_CONFIG_WRITE((1<<31) | (1<<2) | TX_32_BIT<<3 | CLK_DIV_8) // enable trigger on read
        `AHB_SPI_DATA_WRITE(32'h12345678)
        `AHB_SPI_DATA_WRITE(32'hDEADBEEF) // this should stall until the first transaction completes
        `AHB_SPI_IDLE
        assert (slave_shift_register == 32'h12345678);
        @(posedge IRQ); // wait for the first transaction to complete
        assert (slave_shift_register == 32'hDEADBEEF);

        $stop;
    end
endmodule