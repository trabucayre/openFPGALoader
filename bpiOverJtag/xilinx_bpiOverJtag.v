`default_nettype none
/*
 * BPI Flash over JTAG for Xilinx 7-series FPGAs
 * Uses BSCANE2 primitive to access USER1 JTAG register
 */

module bpiOverJtag (
	/* BPI Flash interface */
	output wire [25:1] bpi_addr,
	inout  wire [15:0] bpi_dq,
	/* BPI and SPI flash chip select (active Low). */
	output wire        bpi_ce_n,
	/* BPI flash output enable (active Low). */
	output wire        bpi_oe_n,
	/* BPI flash write enable (active Low). */
	output wire        bpi_we_n,
	/* BPI Flash address valid output (active Low). */
	output wire        bpi_adv_n
);

	wire capture, drck, sel, update, shift;
	wire tdi, tdo;

	/* Version Interface */
	wire ver_sel, ver_cap, ver_shift, ver_drck, ver_tdi, ver_tdo;

	bpiOverJtag_core bpiOverJtag_core_inst (
		/* JTAG state/controls */
		.sel(sel),
		.capture(capture),
		.update(update),
		.shift(shift),
		.drck(drck),
		.tdi(tdi),
		.tdo(tdo),

		/* Version endpoint */
		.ver_sel(ver_sel),
		.ver_cap(ver_cap),
		.ver_shift(ver_shift),
		.ver_drck(ver_drck),
		.ver_tdi(ver_tdi),
		.ver_tdo(ver_tdo),

		/* BPI Flash physical interface */
		.bpi_addr(bpi_addr),
		.bpi_dq(bpi_dq),
		.bpi_ce_n(bpi_ce_n),
		.bpi_oe_n(bpi_oe_n),
		.bpi_we_n(bpi_we_n),
		.bpi_adv_n(bpi_adv_n)
	);

	/* BSCANE2 for main data interface (USER1) */
`ifdef virtex6
	BSCAN_VIRTEX6 #(
`elsif spartan6
	BSCAN_SPARTAN6 #(
`else
	BSCANE2 #(
`endif
		.JTAG_CHAIN(1)
	) bscane2_inst (
		.CAPTURE(capture),
		.DRCK(drck),
		.RESET(),
		.RUNTEST(),
		.SEL(sel),
		.SHIFT(shift),
		.TCK(),
		.TDI(tdi),
		.TMS(),
		.UPDATE(update),
		.TDO(tdo)
	);

	/* BSCANE2 for version interface (USER4) */
`ifdef virtex6
	BSCAN_VIRTEX6 #(
`elsif spartan6
	BSCAN_SPARTAN6 #(
`else
	BSCANE2 #(
`endif
		.JTAG_CHAIN(4)
	) bscane2_version (
		.CAPTURE(ver_cap),
		.DRCK(ver_drck),
		.RESET(),
		.RUNTEST(),
		.SEL(ver_sel),
		.SHIFT(ver_shift),
		.TCK(),
		.TDI(ver_tdi),
		.TMS(),
		.UPDATE(),
		.TDO(ver_tdo)
	);

endmodule
