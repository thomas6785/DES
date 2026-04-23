// This is a module to convert binary to binary coded decimal values
// TODO, this should also handle negative numbers with a flag, by checking the MSB of the input. 


module bin2bcd #(
    localparam INPUT_WIDTH = 10, // including a sign bit
    localparam OUTPUT_WIDTH = INPUT_WIDTH+(INPUT_WIDTH-4)/3+1
)(  input wire signed [INPUT_WIDTH-1: 0] bin_in,
    output reg  [OUTPUT_WIDTH-1:0] bcd_out,
    output reg                      negative_flag);

    integer i, j;
    always @(bin_in) begin
        bcd_out = {OUTPUT_WIDTH{1'b0}};
        negative_flag = 0;

        if (bin_in[INPUT_WIDTH-1] == 1) begin
            negative_flag = 1;
            bcd_out[INPUT_WIDTH-1:0] = -bin_in;
        end else begin
            bcd_out[INPUT_WIDTH-1:0] = bin_in;
        end

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