`default_nettype none
/*
 * JTAG: rising edge: sampling
 *       falling esge: update
 * SPI:
 */

module spiOverJtag_core (
	/* JTAG state/controls */
	input  wire sel,
	input  wire capture,
	input  wire update,
	input  wire shift,
	input  wire drck,
	input  wire tdi,
	output wire tdo,

	/* JTAG endpoint to version */
	input  wire ver_sel,
	input  wire ver_cap,
	input  wire ver_shift,
	input  wire ver_drck,
	input  wire ver_tdi,
	output wire ver_tdo,
	/* phys */
	output reg  csn,
	output wire sck,
	output reg  sdi_dq0,
	input  wire sdo_dq1,
	output wire wpn_dq2,
	output wire hldn_dq3,

	/* debug signals */
	output wire [ 6:0] dbg_header1,
	output wire [13:0] dbg_header,
	output wire [ 3:0] dbg_hdr_cnt,
	output wire [ 2:0] dbg_jtag_state,
	output wire        dbg_rst,
	output wire        dbg_clk,
	output wire        dbg_start_header,

    output wire        dbg_ver_start,
    output wire        dbg_ver_rst,
    output wire        dbg_ver_state,
    output wire [15:0] dbg_ver_cnt,
    output wire [39:0] dbg_ver_shft
);

/* no global reset at start time:
 * reset system at capture time (before shift)
 * and at update time (after shift)
 */
wire rst = ((capture | update) & sel);

/*
 * if the FPGAs executing this code is somewhere in a complex
 * JTAG chain first bit isn't necessary for him
 * Fortunately dummy bits sent are equal to '0' -> we sent a 'start bit'
 */
wire start_header = (tdi & shift & sel);

localparam hdr_len = 16;
reg  [hdr_len-1:0] header;     /* number of bits to receive / send in XFER state */
reg  [hdr_len-1:0] header_d;
/* Primary header with mode and length LSB
 * 6:5: mode (00: normal, 01: no header2, 10: infinite loop)
 * 4:0: Byte length LSB
 */
reg  [        6:0] header1;
reg  [        6:0] header1_d;
wire [        6:0] header1_next = {tdi, header1[6:1]};
wire [        1:0] mode         = header1[1:0];
/* Secondary header with extended length */
wire [hdr_len-1:0] header_next = {tdi, header[hdr_len-1:1]};
reg  [        3:0] hdr_cnt; /* counter of bit received in RECV_HEADERx states */
reg  [        3:0] hdr_cnt_d;

/* ---------------- */
/*       FSM        */
/* ---------------- */

localparam IDLE  = 3'b000,
	RECV_HEADER1 = 3'b001,
	RECV_HEADER2 = 3'b010,
	XFER         = 3'b011,
	WAIT_END     = 3'b100;

reg [2:0] jtag_state, jtag_state_d;

/*
 * 1. receives 8bits (
 */
always @(*) begin
	jtag_state_d = jtag_state;
	hdr_cnt_d    = hdr_cnt;
	header_d     = header;
	header1_d    = header1;
	case (jtag_state)
		IDLE: begin /* nothing: wait for the 'start bit' */
			hdr_cnt_d = 6;
			if (start_header) begin
				jtag_state_d = RECV_HEADER1;
			end
		end
		RECV_HEADER1: begin /* first header with 1:0 : mode, 6:2: XFER length (LSB) */
			hdr_cnt_d = hdr_cnt - 1'b1;
			header1_d = header1_next;
			if (hdr_cnt == 0) begin
				if (header1_next[1:0] == 2'b00) begin
					hdr_cnt_d    = 7;
					header_d     = {header1_next[6:2], 3'b000, 8'd0};
					jtag_state_d = RECV_HEADER2;
				end else begin
					header_d     = {8'b0, header1_next[6:2], 3'b000};
					jtag_state_d = XFER;
				end
			end
		end

		RECV_HEADER2: begin /* fill a counter with 16bits (number of bits to pass to the flash) */
			hdr_cnt_d = hdr_cnt - 1'b1;
			header_d  = header_next;
			if (hdr_cnt == 0) begin
				jtag_state_d = XFER;
			end
		end
		XFER: begin
			header_d = header - 1;
			if (header == 1 && mode != 2'b10)
				jtag_state_d = WAIT_END;
		end
		WAIT_END: begin /* move to this state when header bits have been transfered to the SPI flash */
		//	/* nothing to do: rst will move automagically state in IDLE */
		end
		default: begin
			jtag_state_d = IDLE;
		end
	endcase
end

always @(posedge drck) begin
	header  <= header_d;
	header1 <= header1_d;
	hdr_cnt <= hdr_cnt_d;
end

always @(posedge drck or posedge rst) begin
	if (rst) begin
		jtag_state <= IDLE;
	end else begin
		jtag_state <= jtag_state_d;
	end
end

/* JTAG <-> phy SPI */
always @(posedge drck or posedge rst) begin
	if (rst) begin
		sdi_dq0 <= 1'b0;
		csn     <= 1'b1;
	end else begin
		sdi_dq0 <= tdi;
		csn     <= ~(jtag_state == XFER);
	end
end

assign sck      = ~drck;
assign tdo      = sdo_dq1;
assign wpn_dq2  = 1'b1;
assign hldn_dq3 = 1'b1;

/* ------------- */
/*    Version    */
/* ------------- */
/* no global reset at start time: reset system at capture time (before shift) */
wire ver_rst = (ver_cap & ver_sel);

/* start bit */
wire ver_start = (ver_tdi & ver_shift & ver_sel);

localparam VER_VALUE = 40'h30_30_2E_32_30; // 02.00
reg [ 6:0] ver_cnt, ver_cnt_d;
reg [39:0] ver_shft, ver_shft_d;

reg [2:0] ver_state, ver_state_d;
always @(*) begin
	ver_state_d = ver_state;
	ver_cnt_d   = ver_cnt;
    ver_shft_d  = ver_shft;
	case (ver_state)
		IDLE: begin /* nothing: wait for the 'start bit' */
			ver_cnt_d = 6;
			if (ver_start) begin
				ver_state_d = RECV_HEADER1;
			end
		end
		RECV_HEADER1: begin
			ver_cnt_d = ver_cnt - 1'b1;
			if (ver_cnt == 0) begin
				ver_state_d = XFER;
				ver_cnt_d   = 39;
				ver_shft_d  = VER_VALUE;
			end
		end
		XFER: begin
			ver_cnt_d  = ver_cnt - 1;
			ver_shft_d = {1'b1, ver_shft[39:1]};
			if (ver_cnt == 0)
				ver_state_d = WAIT_END;
		end
		WAIT_END: begin /* move to this state when header bits have been transfered to the SPI flash */
		//  /* nothing to do: rst will move automagically state in IDLE */
		end
		default: begin
			ver_state_d = IDLE;
		end
	endcase
end

always @(posedge ver_drck) begin
	ver_cnt  <= ver_cnt_d;
	ver_shft <= ver_shft_d;
end

always @(posedge ver_drck or posedge ver_rst) begin
	if (ver_rst)
		ver_state <= IDLE;
	else
		ver_state <= ver_state_d;
end

assign ver_tdo = ver_shft[0];

/* --------- */
/*   debug   */
/* --------- */
assign dbg_header1      = header1;
assign dbg_header       = header;
assign dbg_hdr_cnt      = hdr_cnt;
assign dbg_jtag_state   = jtag_state;
assign dbg_rst          = rst;
assign dbg_clk          = ~drck;
assign dbg_start_header = start_header;

assign dbg_ver_start    = ver_start;
assign dbg_ver_state    = ver_state;
assign dbg_ver_cnt      = ver_cnt;
assign dbg_ver_shft     = ver_shft;
assign dbg_ver_rst      = ver_rst;

endmodule
