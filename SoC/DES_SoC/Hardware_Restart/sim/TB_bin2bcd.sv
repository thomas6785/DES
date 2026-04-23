module TB_bin2bcd;

    logic [11:0] dut_out;
    logic [9:0] dut_in;
    logic negative_flag;

    bin2bcd dut (
        .bin_in(dut_in),
        .bcd_out(dut_out),
        .negative_flag
    );

    initial begin
        dut_in = 123;
        #100
        dut_in = 456;
        #100
        dut_in = -5;
        #100
        dut_in = -512;
        #100
        dut_in = 511;
        #100
        $stop;
    end
endmodule