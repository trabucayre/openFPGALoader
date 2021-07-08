module spiOverJtag ();
	wire       tdi, tdo, tck;
	wire [8:0] ir_in;
	wire       vs_cdr, vs_sdr, vs_uir;

	sld_virtual_jtag #(.sld_auto_instance_index("YES"),
		.sld_instance_index(0), .sld_ir_width (9)
	) jtag_ctrl (
		.tdi(tdi), .tdo(tdo), .tck(tck), .ir_in(ir_in),
		.virtual_state_cdr(vs_cdr), .virtual_state_sdr(vs_sdr),
		.virtual_state_uir(vs_uir));

	wire spi_csn, spi_si, spi_clk, spi_so;

	altserial_flash_loader #( 
`ifdef cyclone10lp
		.INTENDED_DEVICE_FAMILY  ("Cyclone 10 LP"),
`elsif cycloneive
		.INTENDED_DEVICE_FAMILY  ("Cyclone IV E"),
`elsif cyclonev
		.INTENDED_DEVICE_FAMILY  ("Cyclone V"),
`endif
		.ENHANCED_MODE           (1),
		.ENABLE_SHARED_ACCESS    ("ON"),
		.ENABLE_QUAD_SPI_SUPPORT (0),
		.NCSO_WIDTH              (1)
	) serial_flash_loader (
		.dclkin(spi_clk), .scein(spi_csn), .sdoin(spi_si), .data0out(spi_so),
		.data_in(), .data_oe(), .data_out(), .noe(1'b0),
		.asmi_access_granted (1'b1), .asmi_access_request ()
	);

	/* vs_uir is used to send
	 * command to the flash
	 * and number of byte to generate
	 */
	reg [7:0] spi_cmd_s;
	reg sdr_d, cdr_d;
	always @(negedge tck) begin
		if (vs_uir) begin
			spi_cmd_s <= ir_in[7:0];
		end
		/* virtual state are updated on rising edge
		*                and sampled at falling edge
		*  => latch on negedge to use after on falling
		*/
		sdr_d <= vs_sdr;
		cdr_d <= vs_cdr;
	end

	/* data in vs_sdr must be sampled on
	* rising edge but use state dealyed by
	* 1/2 clock cycle
	*/
	reg [7:0] test_s;
	reg tdi_d0_s;
	always @(posedge tck) begin
		if (cdr_d) begin
			test_s <= spi_cmd_s;
		end else if (sdr_d) begin
			test_s <= {tdi, test_s[7:1]};
		end
		tdi_d0_s <= tdi;
	end

	reg spi_si_d;
	always @(negedge tck) begin
		if (vs_sdr | sdr_d)
			spi_si_d <= test_s[0];
	end

	assign spi_csn = !sdr_d;
	assign spi_si  = (sdr_d) ? spi_si_d : 1'b0;
	assign spi_clk = sdr_d   ? tck      : 1'b0;
	assign tdo     = sdr_d   ? spi_so   : tdi_d0_s;
endmodule
