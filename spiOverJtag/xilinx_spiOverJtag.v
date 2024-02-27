module spiOverJtag
(
`ifdef spartan6
	output sck,
	output csn,
	output sdi_dq0,
	input  sdo_dq1,
	output wpn_dq2,
	output hldn_dq3
`define QSPI
`elsif spartan3e
	output sck,
	output csn,
	output sdi_dq0,
	input  sdo_dq1
`elsif xilinxultrascale
`else
 // Xilinx 7 but not ultrascale
	output csn,
	output sdi_dq0,
	input  sdo_dq1,
	output wpn_dq2,
	output hldn_dq3
`define QSPI
`endif

`ifdef secondaryflash
	output sdi_sec_dq0,
	input sdo_sec_dq1,
	output wpn_sec_dq2,
	output hldn_sec_dq3,
	output csn_sec
`endif // secondaryflash
);

	wire capture, drck, sel, update;
	wire runtest;
	wire tdi;
	reg fsm_csn;

`ifdef QSPI
        assign wpn_dq2  = 1'b1;
        assign hldn_dq3 = 1'b1;
`endif
	// jtag -> spi flash
	assign sdi_dq0 = tdi;
	wire tdo = (sel) ? sdo_dq1 : tdi;
	assign  csn = fsm_csn;

	wire tmp_cap_s = capture && sel;
	wire tmp_up_s = update && sel;

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

`ifdef xilinxultrascale
	wire [3:0] di;
	assign wpn_dq2  = 1'b1;
	assign hldn_dq3 = 1'b1;
	assign sdo_dq1 = di[1];
	wire [3:0] do = {hldn_dq3, wpn_dq2, 1'b0, sdi_dq0};
	wire [3:0] dts = 4'b0010;
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
`elsif  spartan3e
	assign sck = drck;
	assign runtest = tmp_up_s;
`elsif spartan6
	assign sck = drck;
`else
	STARTUPE2 #(
		.PROG_USR("FALSE"),  // Activate program event security feature. Requires encrypted bitstreams.
		.SIM_CCLK_FREQ(0.0)  // Set the Configuration Clock Frequency(ns) for simulation.
	) startupe2_inst (
		.CFGCLK   (),     // 1-bit output: Configuration main clock output
		.CFGMCLK  (),     // 1-bit output: Configuration internal oscillator clock output
		.EOS      (),     // 1-bit output: Active high output signal indicating the End Of Startup.
		.PREQ     (),     // 1-bit output: PROGRAM request to fabric output
		.CLK      (1'b0), // 1-bit input: User start-up clock input
		.GSR      (1'b0), // 1-bit input: Global Set/Reset input (GSR cannot be used for the port name)
		.GTS      (1'b0), // 1-bit input: Global 3-state input (GTS cannot be used for the port name)
		.KEYCLEARB(1'b0), // 1-bit input: Clear AES Decrypter Key input from Battery-Backed RAM (BBRAM)
		.PACK     (1'b1), // 1-bit input: PROGRAM acknowledge input
		.USRCCLKO (drck), // 1-bit input: User CCLK input
		.USRCCLKTS(1'b0), // 1-bit input: User CCLK 3-state enable input
		.USRDONEO (1'b1), // 1-bit input: User DONE pin output control
		.USRDONETS(1'b1)  // 1-bit input: User DONE 3-state enable output
	);
`endif

`ifdef spartan3e
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
`ifdef spartan6
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
		.RUNTEST(runtest), // 1-bit output: Output asserted when TAP
						   //               controller is in Run Test/Idle state.
		.SEL	 (sel),    // 1-bit output: USER instruction active output.
		.SHIFT   (),       // 1-bit output: SHIFT output from TAP controller.
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
`endif

`ifdef secondaryflash
	reg fsm_csn_sec;
	wire tdo_sec;
	// secondary BSCANE3 signals
	wire sel_sec, drck_sec;

	wire sck = (sel_sec) ? drck_sec : drck;

	assign wpn_sec_dq2  = 1'b1;
	assign hldn_sec_dq3 = 1'b1;
	assign sdi_sec_dq0 = tdi;
	assign tdo_sec = (sel_sec) ? sdo_sec_dq1 : tdi;
	assign  csn_sec = fsm_csn_sec;

	wire tmp_cap_sec_s = capture && sel_sec;
	wire tmp_up_sec_s = update && sel_sec;

	always @(posedge drck_sec, posedge runtest) begin
		if (runtest) begin
			fsm_csn_sec <= 1'b1;
		end else begin
			if (tmp_cap_sec_s) begin
				fsm_csn_sec <= 1'b0;
			end else if (tmp_up_sec_s) begin
				fsm_csn_sec <= 1'b1;
			end else begin
				fsm_csn_sec <= fsm_csn_sec;
			end
		end
	end

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
