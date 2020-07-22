`timescale 1ns / 1ps

module uzedtest(input wire user_sys_clk_p,
           input wire user_sys_clk_n,
           output wire PL_LED1,
           output wire PL_LED2,
           output wire PL_LED3,
           output wire PL_LED4,
           output wire PL_LED5,
           output wire PL_LED6,
           output wire PL_LED7,
           output wire PL_LED8
           );

  wire sysclock;

  IBUFGDS clk_inst (
    .O(sysclock),
    .I(user_sys_clk_p),
    .IB(user_sys_clk_n)
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

  always @(posedge sysclock) begin
      counter <= counter + 64'b1;
  end


endmodule
