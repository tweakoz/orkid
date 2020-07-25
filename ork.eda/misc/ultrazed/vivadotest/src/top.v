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

  reg [9:0] sdi_ycbcr_data;
  reg [2:0] sdi_subclock;
  wire [19:0] sdi_tx_output;
  wire sdi_tx_error; // connect to tranciever

  systemclocks clocks(
    .clk_out1(clk_400_000),
    .clk_out2(clk_400_180),
    .reset(RESET),
    .locked(locked),
    .clk_in1_p(SYSCLK_P),
    .clk_in1_n(SYSCLK_N)
    );

  sdiio sdiout(
    .tx_mode(2'b01), // SD-SDI
    .tx_rst(RESET),
    .tx_usrclk(clk_400_180), // sdi clock (148.5 MHz)
    .tx_video_a_y_in(sdi_ycbcr_data),
    .tx_ce(sdi_subclock),
    .tx_txdata(sdi_tx_output),
    .tx_ce_align_err(sdi_tx_error),
    .tx_use_dsin(1'b0),
    .tx_din_rdy(1'b1),
    .tx_vpid_byte1(8'b0),
    .tx_vpid_byte2(8'b0),
    .tx_vpid_byte3(8'b0),
    .tx_vpid_byte4a(8'b0),
    .tx_vpid_byte4b(8'b0),
    .tx_sd_bitrep_bypass(1'b0),
    .tx_insert_ln(1'b0),
    .tx_insert_crc(1'b0),
    .tx_insert_vpid(1'b0),
    .tx_insert_edh(1'b0)
  );

  reg [63:0] counter;

  initial begin
      counter <= 64'b0;
      sdi_subclock<=3'b0;
      sdi_ycbcr_data<=10'b0;
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
