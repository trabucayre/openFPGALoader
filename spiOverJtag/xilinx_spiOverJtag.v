`default_nettype none
module spiOverJtag
(
`ifndef xilinxultrascale
	output wire csn,

`ifdef spartan6
	output wire sck,
`endif
`ifdef spartan3e
	output wire sck,
`endif
`ifdef virtex6
	output wire sdi_dq0
`else
	output wire sdi_dq0,
	input  wire sdo_dq1,
	output wire wpn_dq2,
	output wire hldn_dq3
`endif
`endif // xilinxultrascale

`ifdef secondaryflash
	output wire sdi_sec_dq0,
	input  wire sdo_sec_dq1,
	output wire wpn_sec_dq2,
	output wire hldn_sec_dq3,
	output wire csn_sec
`endif // secondaryflash
);

	wire capture, drck, sel, update, shift;
	wire tdi, tdo;

`ifndef spartan3e
`ifndef virtex6
	/* Version Interface. */
	wire ver_sel, ver_cap, ver_shift, ver_drck, ver_tdi, ver_tdo;
	wire spi_clk;

	spiOverJtag_core spiOverJtag_core_prim (
		/* JTAG state/controls */
		.sel(sel),
		.capture(capture),
		.update(update),
		.shift(shift),
		.drck(drck),
		.tdi(tdi),
		.tdo(tdo),

		/* JTAG endpoint to version */
		.ver_sel(ver_sel),
		.ver_cap(ver_cap),
		.ver_shift(ver_shift),
		.ver_drck(ver_drck),
		.ver_tdi(ver_tdi),
		.ver_tdo(ver_tdo),

		/* phys */
		.csn(csn),
		.sck(spi_clk),
		.sdi_dq0(sdi_dq0),
		.sdo_dq1(sdo_dq1),
		.wpn_dq2(wpn_dq2),
		.hldn_dq3(hldn_dq3)
	);
`endif /* !virtex6 */
`endif /* !spartan3e */

`ifdef spartan6
	assign sck = spi_clk;
`else // !spartan6
`ifdef spartan3e
	assign sck = spi_drck;
`else // !spartan6 && !spartan3e
`ifdef xilinxultrascale
	assign sck = drck;
	wire [3:0] di;
	assign sdo_dq1 = di[1];
	wire [3:0] do = {hldn_dq3, wpn_dq2, 1'b0, sdi_dq0};
	wire [3:0] dts = 4'b0010;
	// secondary BSCANE3 signals
	wire sel_sec, spi_clk_sec;

	wire sck = (sel_sec) ? spi_clk_sec : spi_clk;

	STARTUPE3 #(
		.PROG_USR("FALSE"),  // Activate program event security feature. Requires encrypted bitstreams.
		.SIM_CCLK_FREQ(0.0)  // Set the Configuration Clock Frequency (ns) for simulation.
	) startupe3_inst (
		.CFGCLK   (),     // 1-bit output: Configuration main clock output.
		.CFGMCLK  (),     // 1-bit output: Configuration internal oscillator clock output.
		.DI       (di),   // 4-bit output: Allow receiving on the D input pin.
		.EOS      (),     // 1-bit output: Active-High output signal indicating the End Of Startup.
		.PREQ     (),     // 1-bit output: PROGRAM request to fabric output.
		.DO       (do),   // 4-bit input: Allows control of the D pin output.
		.DTS      (dts),  // 4-bit input: Allows tristate of the D pin.
		.FCSBO    (csn),  // 1-bit input: Controls the FCS_B pin for flash access.
		.FCSBTS   (1'b0), // 1-bit input: Tristate the FCS_B pin.
		.GSR      (1'b0), // 1-bit input: Global Set/Reset input (GSR cannot be used for the port).
		.GTS      (1'b0), // 1-bit input: Global 3-state input (GTS cannot be used for the port name).
		.KEYCLEARB(1'b0), // 1-bit input: Clear AES Decrypter Key input from Battery-Backed RAM (BBRAM).
		.PACK     (1'b0), // 1-bit input: PROGRAM acknowledge input.
		.USRCCLKO (sck),  // 1-bit input: User CCLK input.
		.USRCCLKTS(1'b0), // 1-bit input: User CCLK 3-state enable input.
		.USRDONEO (1'b1), // 1-bit input: User DONE pin output control.
		.USRDONETS(1'b1)  // 1-bit input: User DONE 3-state enable output.
	);
`elsif virtex6 // !spartan6 && !spartan3e && !xilinxultrascale
	wire di;

	wire runtest;
	reg fsm_csn;
	// jtag -> spi flash
	assign sdi_dq0 = tdi;
	assign tdo     = (sel) ? di : tdi;
	assign csn     = fsm_csn;

	wire tmp_cap_s = capture && sel;
	wire tmp_up_s  = update && sel;

	always @(posedge drck, posedge runtest) begin
		if (runtest) begin
			fsm_csn <= 1'b1;
		end else begin
			if (tmp_cap_s) begin
				fsm_csn <= 1'b0;
			end else if (tmp_up_s) begin
				fsm_csn <= 1'b1;
			end else begin
				fsm_csn <= fsm_csn;
			end
		end
	end
	STARTUP_VIRTEX6 #(
			.PROG_USR("FALSE")
	) startup_virtex6_inst (
			.CFGCLK(),        // unused
			.CFGMCLK(),       // unused
			.CLK(1'b0),       // unused
			.DINSPI(di),      // data from SPI flash
			.EOS(),
			.GSR(1'b0),       // unused
			.GTS(1'b0),       // unused
			.KEYCLEARB(1'b0),  // not used
			.PACK(1'b1),      // tied low for 'safe' operations
			.PREQ(),          // unused
			.TCKSPI(),        // echo of CCLK from TCK pin
			.USRCCLKO (drck), // user FPGA -> CCLK pin
			.USRCCLKTS(1'b0), // drive CCLK not in high-Z
			.USRDONEO (1'b1), // why both USRDONE are high?
			.USRDONETS(1'b1)  // ??
	);
`else // !spartan6 && !spartan3e && !xilinxultrascale && !virtex6
	STARTUPE2 #(
		.PROG_USR("FALSE"),  // Activate program event security feature. Requires encrypted bitstreams.
		.SIM_CCLK_FREQ(0.0)  // Set the Configuration Clock Frequency(ns) for simulation.
	) startupe2_inst (
		.CFGCLK   (),        // 1-bit output: Configuration main clock output
		.CFGMCLK  (),        // 1-bit output: Configuration internal oscillator clock output
		.EOS      (),        // 1-bit output: Active high output signal indicating the End Of Startup.
		.PREQ     (),        // 1-bit output: PROGRAM request to fabric output
		.CLK      (1'b0),    // 1-bit input: User start-up clock input
		.GSR      (1'b0),    // 1-bit input: Global Set/Reset input (GSR cannot be used for the port name)
		.GTS      (1'b0),    // 1-bit input: Global 3-state input (GTS cannot be used for the port name)
		.KEYCLEARB(1'b0),    // 1-bit input: Clear AES Decrypter Key input from Battery-Backed RAM (BBRAM)
		.PACK     (1'b1),    // 1-bit input: PROGRAM acknowledge input
		.USRCCLKO (spi_clk), // 1-bit input: User CCLK input
		.USRCCLKTS(1'b0),    // 1-bit input: User CCLK 3-state enable input
		.USRDONEO (1'b1),    // 1-bit input: User DONE pin output control
		.USRDONETS(1'b1)     // 1-bit input: User DONE 3-state enable output
	);
`endif
`endif
`endif

`ifdef spartan3e
	wire runtest;
	reg fsm_csn;
	assign wpn_dq2  = 1'b1;
	assign hldn_dq3 = 1'b1;
	// jtag -> spi flash
	assign sdi_dq0 = tdi;
	assign tdo     = (sel) ? sdo_dq1 : tdi;
	assign csn     = fsm_csn;

	wire tmp_cap_s = capture && sel;
	wire tmp_up_s = update && sel;
	assign runtest = tmp_up_s;

	always @(posedge drck, posedge runtest) begin
		if (runtest) begin
			fsm_csn <= 1'b1;
		end else begin
			if (tmp_cap_s) begin
				fsm_csn <= 1'b0;
			end else if (tmp_up_s) begin
				fsm_csn <= 1'b1;
			end else begin
				fsm_csn <= fsm_csn;
			end
		end
	end

	BSCAN_SPARTAN3 bscane2_inst (
		.CAPTURE(capture), // 1-bit output: CAPTURE output from TAP controller.
		.DRCK1	(drck),    // 1-bit output: Gated TCK output. When SEL
						   //               is asserted, DRCK toggles when
						   //               CAPTURE or SHIFT are asserted.
		.DRCK2  (),        // 1-bit output: USER2 function
		.RESET  (),        // 1-bit output: Reset output for TAP controller.
		.SEL1   (sel),     // 1-bit output: USER1 instruction active output.
		.SEL2   (),        // 1-bit output: USER2 instruction active output.
		.SHIFT  (),        // 1-bit output: SHIFT output from TAP controller.
		.TDI    (tdi),     // 1-bit output: Test Data Input (TDI) output
		                   //               from TAP controller.
		.UPDATE (update),  // 1-bit output: UPDATE output from TAP controller
		.TDO1   (tdo),     // 1-bit input: Test Data Output (TDO) input
		                   //              for USER1 function.
		.TDO2   ()         // 1-bit input: USER2 function
	);
`else
`ifdef virtex6
	BSCAN_VIRTEX6 #(
`elsif spartan6
	BSCAN_SPARTAN6 #(
`else
	BSCANE2 #(
`endif
		.JTAG_CHAIN(1)  // Value for USER command.
	) bscane2_inst (
		.CAPTURE(capture), // 1-bit output: CAPTURE output from TAP controller.
		.DRCK	(drck),    // 1-bit output: Gated TCK output. When SEL
						   //               is asserted, DRCK toggles when
						   //               CAPTURE or SHIFT are asserted.
		.RESET  (),        // 1-bit output: Reset output for TAP controller.
`ifdef virtex6
		.RUNTEST(runtest),
`else
		.RUNTEST(),        // 1-bit output: Output asserted when TAP
						   //               controller is in Run Test/Idle state.
`endif
		.SEL	 (sel),    // 1-bit output: USER instruction active output.
		.SHIFT   (shift),  // 1-bit output: SHIFT output from TAP controller.
		.TCK     (),       // 1-bit output: Test Clock output.
		                   //               Fabric connection to TAP Clock pin.
		.TDI     (tdi),    // 1-bit output: Test Data Input (TDI) output
		                   //               from TAP controller.
		.TMS     (),       // 1-bit output: Test Mode Select output.
		                   //               Fabric connection to TAP.
		.UPDATE  (update), // 1-bit output: UPDATE output from TAP controller
		.TDO     (tdo)     // 1-bit input: Test Data Output (TDO) input
		                   //              for USER function.
	);

/* BSCAN for Version Interface. */
`ifndef virtex6
`ifdef spartan6
	BSCAN_SPARTAN6 #(
`else
	BSCANE2 #(
`endif
		.JTAG_CHAIN(4)  // Value for USER command.
	) bscane2_version (
		.CAPTURE(ver_cap),
		.DRCK	(ver_drck),
		.RESET  (),
		.RUNTEST(),
		.SEL	 (ver_sel),
		.SHIFT   (ver_shift),
		.TCK     (),
		.TDI     (ver_tdi),
		.TMS     (),
		.UPDATE  (),
		.TDO     (ver_tdo)
	);
`endif
`endif /* !virtex6 */

`ifdef secondaryflash
	wire drck_sec;

	spiOverJtag_core spiOverJtag_core_sec (
		/* JTAG state/controls */
		.sel(sel_sec),
		.capture(capture),
		.update(update),
		.shift(shift),
		.drck(drck_sec),
		.tdi(tdi),
		.tdo(tdo_sec),

		/* JTAG endpoint to version (Unused) */
		.ver_sel(1'b0),
		.ver_cap(1'b0),
		.ver_shift(1'b0),
		.ver_drck(1'b0),
		.ver_tdi(1'b0),
		.ver_tdo(),

		/* phys */
		.csn(csn_sec),
		.sck(spi_clk_sec),
		.sdi_dq0(sdi_sec_dq0),
		.sdo_dq1(sdo_sec_dq1),
		.wpn_dq2(wpn_sec_dq2),
		.hldn_dq3(hldn_sec_dq3)
	);

	BSCANE2 #(
		.JTAG_CHAIN(2)  // Value for USER command.
	) bscane2_sec_inst (
		.CAPTURE(), // 1-bit output: CAPTURE output from TAP controller.
		.DRCK	(drck_sec),    // 1-bit output: Gated TCK output. When SEL
						   //               is asserted, DRCK toggles when
						   //               CAPTURE or SHIFT are asserted.
		.RESET  (),        // 1-bit output: Reset output for TAP controller.
		.RUNTEST(), // 1-bit output: Output asserted when TAP
						   //               controller is in Run Test/Idle state.
		.SEL	 (sel_sec),    // 1-bit output: USER instruction active output.
		.SHIFT   (),       // 1-bit output: SHIFT output from TAP controller.
		.TCK     (),       // 1-bit output: Test Clock output.
		                   //               Fabric connection to TAP Clock pin.
		.TDI     (),    // 1-bit output: Test Data Input (TDI) output
		                   //               from TAP controller.
		.TMS     (),       // 1-bit output: Test Mode Select output.
		                   //               Fabric connection to TAP.
		.UPDATE  (), // 1-bit output: UPDATE output from TAP controller
		.TDO     (tdo_sec)     // 1-bit input: Test Data Output (TDO) input
		                   //              for USER function.
	);
`endif // secondaryflash

endmodule
