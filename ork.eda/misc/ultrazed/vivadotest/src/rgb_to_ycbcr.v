module rgb_to_ycbcr_444(
  input wire in_reset,
  input wire in_clock,
  input wire [7:0] in_r,
  input wire [7:0] in_g,
  input wire [7:0] in_b,
  output reg [7:0] out_y,
  output reg [7:0] out_cb,
  output reg [7:0] out_cr,
  );

  always @(posedge in_clock)begin
    if(in_reset) begin
      out_y <= 0;
      out_cb <= 0;
      out_cr <= 0;
    end
    else begin
      out_y <= 16+(((in_r<<6)+(in_r<<1)+(in_g<<7)+in_g+(in_b<<4)+(in_b<<3)+in_b)>>8);
      out_cb <= 128 + ((-((in_r<<5)+(in_r<<2)+(in_r<<1))-((in_g<<6)+(in_g<<3)+(in_g<<1))+(in_b<<7)-(in_b<<4))>>8);
      out_cr <= 128 + (((in_r<<7)-(in_r<<4)-((in_g<<6)+(in_g<<5)-(in_g<<1))-((in_b<<4)+(in_b<<1)))>>8);
    end
  end

endmodule
