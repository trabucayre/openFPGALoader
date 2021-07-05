library IEEE;
use IEEE.STD_LOGIC_1164.all;
use IEEE.NUMERIC_STD.all;
Library UNISIM;
use UNISIM.vcomponents.all;

entity xilinx_spiOverJtag is
	port (
	 csn       : out std_logic;
	 sdi       : out std_logic;
	 sdo       : in std_logic;
	 sck       : out std_logic;
	 wpn	   : out std_logic;
	 hldn      : out std_logic
	);
end entity xilinx_spiOverJtag;

architecture bhv of xilinx_spiOverJtag is
	signal capture, drck, sel, shift, update : std_logic;
	signal runtest : std_logic;
	signal tdi, tdo   : std_logic;
	signal fsm_csn : std_logic;

	signal tmp_up_s, tmp_shift_s, tmp_cap_s : std_logic;
begin
	wpn <= '1';
	hldn <= '1';
	-- jtag -> spi flash
	csn <= fsm_csn;
	sdi <= tdi;
	tdo <= tdi when (sel) = '0' else sdo;
	sck <= drck;

	tmp_cap_s <= capture and sel;
	tmp_up_s <= update and sel;

	process(drck, runtest) begin
		if runtest = '1' then
			fsm_csn <= '1';
		elsif rising_edge(drck) then
			if tmp_cap_s = '1' then
				fsm_csn <= '0';
			elsif tmp_up_s = '1' then
				fsm_csn <= '1';
			else
				fsm_csn <= fsm_csn;
			end if;
		end if;
	end process;

	BSCAN_SPARTAN6_inst : BSCAN_SPARTAN6
   generic map (
      JTAG_CHAIN => 1  -- Value for USER command. Possible values: (1,2,3 or 4).
   )
   port map (
      CAPTURE => capture, -- 1-bit output: CAPTURE output from TAP controller.
      DRCK => drck,       -- 1-bit output: Data register output for USER functions.
      RUNTEST => runtest, -- 1-bit output: Output signal that gets asserted when TAP controller is in Run Test
                          -- Idle state.

      SEL => sel,         -- 1-bit output: USER active output.
      SHIFT => shift,     -- 1-bit output: SHIFT output from TAP controller.
      TDI => tdi,         -- 1-bit output: TDI output from TAP controller.
      UPDATE => update,   -- 1-bit output: UPDATE output from TAP controller
      TDO => tdo          -- 1-bit input: Data input for USER function.
   );

end architecture bhv;
