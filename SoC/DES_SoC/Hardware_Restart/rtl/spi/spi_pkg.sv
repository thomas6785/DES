`timescale 1ns/1ps

package spi_pkg;
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

    typedef enum bit [1:0] {
        CLK_DIV_4 = 2'b00,
        CLK_DIV_8 = 2'b01,
        CLK_DIV_16 = 2'b10,
        CLK_DIV_32 = 2'b11
    } clk_div_e;
endpackage