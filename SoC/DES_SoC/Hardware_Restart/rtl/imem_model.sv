`timescale 1ns / 1ns

module imem_model (
    input                clka,      // port A is write port
    input                ena,       //
    input                wea,       //
    input        [12:0]  addra,     //
    input        [31:0]  dina,      //
    input                clkb,      // port B is read port
    input                enb,       //
    input        [12:0]  addrb,     //
    output logic [31:0]  doutb      //
);

    logic [31:0] mem [8191:0]; // 8K x 32-bit memory

    // Write logic for port A
    always_ff @(posedge clka) begin
        if (ena && wea) begin
            mem[addra] <= dina; // Write data to memory at address addra
        end
    end

    // Read logic for port B
    always_ff @(posedge clkb) begin
        if (enb) begin
            doutb <= mem[addrb]; // Read data from memory at address addrb
        end
    end
endmodule