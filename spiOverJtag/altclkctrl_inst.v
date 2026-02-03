module altclkctrl_inst (
	input  wire  inclk,
	input  wire  ena,
	output wire  outclk
);
altclkctrl  u (
	.inclk(inclk),
	.ena(ena),
	.outclk(outclk)
);
endmodule
