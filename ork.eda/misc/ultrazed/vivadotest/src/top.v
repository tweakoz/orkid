`timescale 1ns / 1ps

module uzedtest(input wire SYSCLK_P,
           input wire SYSCLK_N,
           input wire RESET,
           input wire GTH_REFCLK0_P, // 148.5 mhz HDMI clock
           input wire GTH_REFCLK0_N, // 148.5 mhz HDMI clock
           output wire U13_SDIP,
           output wire U13_SDIN,
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
  wire [19:0] sdi_tx_output; // connect to tranciever
  wire [31:0] sdi_tx_error;
  reg vid_active;
  reg [59:0] vid_data;
  reg vid_field;
  reg vid_hblank;
  reg vid_vblank;
  wire vid_ce;
  wire [63:0] gt_sts;
  reg [63:0] gt_ctrl;

  //////////////////////////////////////
  // system clocks
  //////////////////////////////////////

  systemclocks clocks(
    .clk_out1(clk_400_000),
    .clk_out2(clk_400_180),
    .reset(RESET),
    .locked(locked),
    .clk_in1_p(SYSCLK_P),
    .clk_in1_n(SYSCLK_N)
    );

  //////////////////////////////////////
  // SDI GT transceiver
  //////////////////////////////////////

  sdigt sdi_gt(
    .intf_0_txp(U13_SDIP),
    .intf_0_txn(U13_SDIN),
    .intf_0_qpll0_refclk_in(GTH_REFCLK0_P),
    .intf_0_qpll1_refclk_in(GTH_REFCLK0_P),
    .cmp_gt_ctrl(gt_ctrl),
    .cmp_gt_sts(gt_sts)
  );

  //////////////////////////////////////
  // SDI controller
  //////////////////////////////////////

  sditx sdi_transmitter(
    .sdi_tx_rst(RESET),
    .sdi_tx_clk(GTH_REFCLK0_P), // sdi clock (148.5 MHz)
    .sdi_tx_ctrl(32'h00000011), // mode(SD-SDI,integer frame rate)
    .sdi_tx_err(sdi_tx_error),
    .VID_IO_IN_active_video(vid_active),
    .VID_IO_IN_data(vid_data),
    .VID_IO_IN_field(vid_field),
    .VID_IO_IN_hblank(vid_hblank),
    .VID_IO_IN_vblank(vid_vblank),
    .vid_ce(vid_ce)
  );

  //////////////////////////////////////
  // custom logic
  //////////////////////////////////////

  //2477475.0 == (148.5e6)/(2*10.0e6*63/88/455/525.0)

  // NTSC LPS == 15734 
  // PAL LPS == 15625

  reg [63:0] counter;

  initial begin
      counter <= 64'b0;
      sdi_subclock<=3'b0;
      sdi_ycbcr_data<=10'b0;
      vid_active<=0;
      vid_field<=0;
      vid_hblank<=0;
      vid_vblank<=0;
      vid_data<=60'b0;
      gt_ctrl<=64'b0;
  end

  assign PL_LED1 = counter[24];
  assign PL_LED2 = counter[25];
  assign PL_LED3 = counter[26];
  assign PL_LED4 = counter[27];
  assign PL_LED5 = counter[28];
  assign PL_LED6 = counter[29];
  assign PL_LED7 = counter[30];
  assign PL_LED8 = counter[31];

  // todo: video timing and data

  always @(posedge clk_400_000 or posedge RESET) begin
    if(RESET || ! locked)
      counter <= 0;
    else
      counter <= counter + 64'b1;
  end


endmodule
