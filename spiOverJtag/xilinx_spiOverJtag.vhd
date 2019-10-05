library IEEE;
use IEEE.STD_LOGIC_1164.all;
use IEEE.NUMERIC_STD.all;
Library UNISIM;
use UNISIM.vcomponents.all;

entity xilinx_spiOverJtag is
	port (
		csn       : out std_logic;
		sdi_dq0   : out std_logic;
		sdo_dq1   : in std_logic;
		wpn_dq2   : out std_logic;
		hldn_dq3  : out std_logic
	);
end entity xilinx_spiOverJtag;

architecture bhv of xilinx_spiOverJtag is
	signal capture, drck, sel, shift, update : std_logic;
	signal runtest : std_logic;
	signal tdi, tdo   : std_logic;
	signal fsm_csn : std_logic;

	signal tmp_up_s, tmp_shift_s, tmp_cap_s : std_logic;
begin
	wpn_dq2 <= '1';
	hldn_dq3 <= '1';
	-- jtag -> spi flash
	sdi_dq0 <= tdi;
	tdo <= tdi when (sel) = '0' else sdo_dq1;
	csn <= fsm_csn;

	tmp_cap_s <= capture and sel;
	tmp_up_s <= update and sel;
	tmp_shift_s <= shift and sel;

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

	startupe2_inst : STARTUPE2
	generic map (
		PROG_USR => "FALSE",  -- Activate program event security feature. Requires encrypted bitstreams.
		SIM_CCLK_FREQ => 0.0  -- Set the Configuration Clock Frequency(ns) for simulation.
	)
	port map (
		CFGCLK    => open, -- 1-bit output: Configuration main clock output
		CFGMCLK   => open, -- 1-bit output: Configuration internal oscillator clock output
		EOS       => open, -- 1-bit output: Active high output signal indicating the End Of Startup.
		PREQ      => open, -- 1-bit output: PROGRAM request to fabric output
		CLK       => '0',  -- 1-bit input: User start-up clock input
		GSR       => '0',  -- 1-bit input: Global Set/Reset input (GSR cannot be used for the port name)
		GTS       => '0',  -- 1-bit input: Global 3-state input (GTS cannot be used for the port name)
		KEYCLEARB => '0',  -- 1-bit input: Clear AES Decrypter Key input from Battery-Backed RAM (BBRAM)
		PACK      => '1',  -- 1-bit input: PROGRAM acknowledge input
		USRCCLKO  => drck, -- 1-bit input: User CCLK input
		USRCCLKTS => '0',  -- 1-bit input: User CCLK 3-state enable input
		USRDONEO  => '1',  -- 1-bit input: User DONE pin output control
		USRDONETS => '1'   -- 1-bit input: User DONE 3-state enable output
	);

   
	bscane2_inst : BSCANE2
		generic map (
			JTAG_CHAIN => 1  -- Value for USER command.
		)
		port map (
			CAPTURE => capture, -- 1-bit output: CAPTURE output from TAP controller.
			DRCK	=> drck,    -- 1-bit output: Gated TCK output. When SEL
								--               is asserted, DRCK toggles when
								--               CAPTURE or SHIFT are asserted.
			RESET   => open,    -- 1-bit output: Reset output for TAP controller.
			RUNTEST => runtest, -- 1-bit output: Output asserted when TAP
								--               controller is in Run Test/Idle state.
			SEL	    => sel,     -- 1-bit output: USER instruction active output.
			SHIFT   => shift,   -- 1-bit output: SHIFT output from TAP controller.
			TCK     => open,    -- 1-bit output: Test Clock output.
			                    --               Fabric connection to TAP Clock pin.
			TDI     => tdi,     -- 1-bit output: Test Data Input (TDI) output
			                    --               from TAP controller.
			TMS     => open,    -- 1-bit output: Test Mode Select output.
			                    --               Fabric connection to TAP.
			UPDATE  => update,  -- 1-bit output: UPDATE output from TAP controller
			TDO     => tdo      -- 1-bit input: Test Data Output (TDO) input
			                    --              for USER function.
		);

end architecture bhv;
