.. _troubleshooting:

Troubleshooting
###############

I installed openFPGALoader but it says `command not found` when I try to launch it
==================================================================================

The correct spelling of the program is *openFPGALoader* with FPGA and the "L" of "Loader" in uppercase.
Ensure the spelling of the program is correct.

Gowin device could not communicate since last bitstream flashed. (issue `#206 <https://github.com/trabucayre/openFPGALoader/issues/206>`_)
==========================================================================================================================================

Gowin's FPGA may fails to be detected if **JTAGSEL_N** (pin 08 for *GW1N-4K*) is used as a GPIO.
To recover you have to pull down this pin (before power up) to recover JTAG interface (*UG292 - JTAGSELL_N section*).
