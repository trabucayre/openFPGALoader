`default_nettype none
/*
 * BPI (Parallel NOR) Flash over JTAG core
 *
 * Protocol (all in one DR shift):
 *   TX: [start=1][cmd:4][addr:25][wr_data:16] = 46 bits
 *   RX: Response appears after command bits, aligned to read_data position
 *
 * Commands:
 *   0x1 = Write word to flash (addr + data)
 *   0x2 = Read word from flash (addr), returns data
 *   0x3 = NOP / get status
 */

module bpiOverJtag_core (
    /* JTAG state/controls */
    input  wire        sel,
    input  wire        capture,
    input  wire        update,
    input  wire        shift,
    input  wire        drck,
    input  wire        tdi,
    output wire        tdo,

    /* Version endpoint */
    input  wire        ver_sel,
    input  wire        ver_cap,
    input  wire        ver_shift,
    input  wire        ver_drck,
    input  wire        ver_tdi,
    output wire        ver_tdo,

    /* BPI Flash physical interface */
    output reg  [25:1] bpi_addr,
    inout  wire [15:0] bpi_dq,
    output reg         bpi_ce_n,
    output reg         bpi_oe_n,
    output reg         bpi_we_n,
    output reg         bpi_adv_n
);

/* Reset on capture when selected */
wire rst = (capture & sel);

/* Start bit detection */
wire start_header = (tdi & shift & sel);

/* State machine */
localparam IDLE      = 3'd0,
           RECV_CMD  = 3'd1,
           RECV_ADDR = 3'd2,
           RECV_DATA = 3'd3,
           EXEC      = 3'd4,
           SEND_DATA = 3'd5,
           DONE      = 3'd6;

reg [2:0] state, state_d;
reg [5:0] bit_cnt, bit_cnt_d;
reg [3:0] cmd_reg, cmd_reg_d;
reg [24:0] addr_reg, addr_reg_d;
reg [15:0] wr_data_reg, wr_data_reg_d;
reg [15:0] rd_data_reg, rd_data_reg_d;
reg [7:0] wait_cnt, wait_cnt_d;

/* Data bus control */
reg dq_oe;
reg [15:0] dq_out;
assign bpi_dq = dq_oe ? dq_out : 16'hzzzz;

/* TDO output - shift out read data */
assign tdo = rd_data_reg[0];

/* Command codes */
localparam CMD_WRITE = 4'h1,
           CMD_READ  = 4'h2,
           CMD_NOP   = 4'h3;

/* Next state logic */
always @(*) begin
    state_d      = state;
    bit_cnt_d    = bit_cnt;
    cmd_reg_d    = cmd_reg;
    addr_reg_d   = addr_reg;
    wr_data_reg_d = wr_data_reg;
    rd_data_reg_d = rd_data_reg;
    wait_cnt_d   = wait_cnt;

    case (state)
        IDLE: begin
            bit_cnt_d = 3;  /* 4 bits for command */
            if (start_header)
                state_d = RECV_CMD;
        end

        RECV_CMD: begin
            cmd_reg_d = {tdi, cmd_reg[3:1]};
            bit_cnt_d = bit_cnt - 1'b1;
            if (bit_cnt == 0) begin
                bit_cnt_d = 24;  /* 25 bits for address */
                state_d = RECV_ADDR;
            end
        end

        RECV_ADDR: begin
            addr_reg_d = {tdi, addr_reg[24:1]};
            bit_cnt_d = bit_cnt - 1'b1;
            if (bit_cnt == 0) begin
                if (cmd_reg == CMD_WRITE) begin
                    bit_cnt_d = 15;  /* 16 bits for data */
                    state_d = RECV_DATA;
                end else begin
                    wait_cnt_d = 8'd20;  /* Wait cycles for read */
                    state_d = EXEC;
                end
            end
        end

        RECV_DATA: begin
            wr_data_reg_d = {tdi, wr_data_reg[15:1]};
            bit_cnt_d = bit_cnt - 1'b1;
            if (bit_cnt == 0) begin
                wait_cnt_d = 8'd20;  /* Wait cycles for write */
                state_d = EXEC;
            end
        end

        EXEC: begin
            wait_cnt_d = wait_cnt - 1'b1;
            if (wait_cnt == 8'd10 && cmd_reg == CMD_READ) begin
                /* Sample read data mid-cycle */
                rd_data_reg_d = bpi_dq;
            end
            if (wait_cnt == 0) begin
                bit_cnt_d = 15;
                state_d = SEND_DATA;
            end
        end

        SEND_DATA: begin
            rd_data_reg_d = {1'b1, rd_data_reg[15:1]};
            bit_cnt_d = bit_cnt - 1'b1;
            if (bit_cnt == 0)
                state_d = DONE;
        end

        DONE: begin
            /* Stay here until reset */
        end

        default: state_d = IDLE;
    endcase
end

/* State register */
always @(posedge drck or posedge rst) begin
    if (rst)
        state <= IDLE;
    else
        state <= state_d;
end

/* Data registers */
always @(posedge drck) begin
    bit_cnt     <= bit_cnt_d;
    cmd_reg     <= cmd_reg_d;
    addr_reg    <= addr_reg_d;
    wr_data_reg <= wr_data_reg_d;
    rd_data_reg <= rd_data_reg_d;
    wait_cnt    <= wait_cnt_d;
end

/* Address output */
always @(posedge drck or posedge rst) begin
    if (rst)
        bpi_addr <= 25'd0;
    else if (state == RECV_ADDR && bit_cnt == 0)
        bpi_addr <= {tdi, addr_reg[24:1]};
end

/* BPI Flash control signals */
always @(posedge drck or posedge rst) begin
    if (rst) begin
        bpi_ce_n  <= 1'b1;
        bpi_oe_n  <= 1'b1;
        bpi_we_n  <= 1'b1;
        bpi_adv_n <= 1'b1;
        dq_oe     <= 1'b0;
        dq_out    <= 16'h0000;
    end else begin
        case (state_d)
            EXEC: begin
                bpi_ce_n  <= 1'b0;
                bpi_adv_n <= 1'b0;
                if (cmd_reg == CMD_READ) begin
                    bpi_oe_n <= 1'b0;
                    bpi_we_n <= 1'b1;
                    dq_oe    <= 1'b0;
                end else if (cmd_reg == CMD_WRITE) begin
                    bpi_oe_n <= 1'b1;
                    bpi_we_n <= (wait_cnt > 8'd5 && wait_cnt < 8'd15) ? 1'b0 : 1'b1;
                    dq_oe    <= 1'b1;
                    dq_out   <= wr_data_reg;
                end
            end
            default: begin
                bpi_ce_n  <= 1'b1;
                bpi_oe_n  <= 1'b1;
                bpi_we_n  <= 1'b1;
                bpi_adv_n <= 1'b1;
                dq_oe     <= 1'b0;
            end
        endcase
    end
end

/* ------------- */
/*    Version    */
/* ------------- */
wire ver_rst = (ver_cap & ver_sel);
wire ver_start = (ver_tdi & ver_shift & ver_sel);

localparam VER_VALUE = 40'h30_31_2E_30_30; // "01.00"

reg [6:0] ver_cnt, ver_cnt_d;
reg [39:0] ver_shft, ver_shft_d;
reg [2:0] ver_state, ver_state_d;

localparam VER_IDLE = 3'd0,
           VER_RECV = 3'd1,
           VER_XFER = 3'd2,
           VER_WAIT = 3'd3;

always @(*) begin
    ver_state_d = ver_state;
    ver_cnt_d   = ver_cnt;
    ver_shft_d  = ver_shft;
    case (ver_state)
        VER_IDLE: begin
            ver_cnt_d = 6;
            if (ver_start)
                ver_state_d = VER_RECV;
        end
        VER_RECV: begin
            ver_cnt_d = ver_cnt - 1'b1;
            if (ver_cnt == 0) begin
                ver_state_d = VER_XFER;
                ver_cnt_d   = 39;
                ver_shft_d  = VER_VALUE;
            end
        end
        VER_XFER: begin
            ver_cnt_d  = ver_cnt - 1;
            ver_shft_d = {1'b1, ver_shft[39:1]};
            if (ver_cnt == 0)
                ver_state_d = VER_WAIT;
        end
        VER_WAIT: begin
            /* Wait for reset */
        end
        default: ver_state_d = VER_IDLE;
    endcase
end

always @(posedge ver_drck) begin
    ver_cnt  <= ver_cnt_d;
    ver_shft <= ver_shft_d;
end

always @(posedge ver_drck or posedge ver_rst) begin
    if (ver_rst)
        ver_state <= VER_IDLE;
    else
        ver_state <= ver_state_d;
end

assign ver_tdo = ver_shft[0];

endmodule
