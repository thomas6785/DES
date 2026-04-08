// This is a module to convert binary to binary coded decimal values

module bin2bcd #(
    parameter INPUT_WIDTH = 8
)(  input       [INPUT_WIDTH -1: 0] bin_in,
    output reg  [INPUT_WIDTH + (INPUT_WIDTH -4)/3: 0] bcd_out);


    integer i, j;
    always @(bin_in) begin
        bcd_out = '0;

        bcd_out[INPUT_WIDTH-1:0]= bin_in;

        for (i=0; i <= INPUT_WIDTH-4; i= i+1)
        begin
            for (j=0; j <= i/3; j= j+1)
            begin
                if(bcd_out[INPUT_WIDTH-i+4*j -: 4] > 4)
                    bcd_out[INPUT_WIDTH-i+4*j -: 4] = bcd_out[INPUT_WIDTH-i+4*j -: 4] + 4'd3;
            end
        end

    end

endmodule