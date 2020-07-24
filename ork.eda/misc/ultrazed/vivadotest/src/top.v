`timescale 1ns / 1ps

module uzedtest(input wire SYSCLK_P,
           input wire SYSCLK_N,
           input wire RESET,
           output wire PL_LED1,
           output wire PL_LED2,
           output wire PL_LED3,
           output wire PL_LED4,
           output wire PL_LED5,
           output wire PL_LED6,
           output wire PL_LED7,
           output wire PL_LED8
           );

  wire clk_400_000;
  wire clk_400_180;
  wire locked;

  systemclocks clocks(
    .clk_out1(clk_400_000),
    .clk_out2(clk_400_180),
    .reset(RESET),
    .locked(locked),
    .clk_in1_p(SYSCLK_P),
    .clk_in1_n(SYSCLK_N)
    );

  reg [63:0] counter;

  initial begin
      counter <= 64'b0;
  end

  assign PL_LED1 = counter[24];
  assign PL_LED2 = counter[25];
  assign PL_LED3 = counter[26];
  assign PL_LED4 = counter[27];
  assign PL_LED5 = counter[28];
  assign PL_LED6 = counter[29];
  assign PL_LED7 = counter[30];
  assign PL_LED8 = counter[31];

  always @(posedge clk_400_000 or posedge RESET) begin
    if(RESET || ! locked)
      counter <= 0;
    else
      counter <= counter + 64'b1;
  end


endmodule
