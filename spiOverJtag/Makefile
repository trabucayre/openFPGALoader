VIVADO := vivado -nolog -nojournal -mode batch -source
MODEL = xc7a35
PRJ = spiOverJtag_$(MODEL)

BIT_PATH     = tmp/$(PRJ).runs/impl_1/
BIT_TMP_FILE = $(BIT_PATH)/*.bit
BIT_FILE = $(PRJ).bit

$(BIT_FILE) : $(BIT_TMP_FILE)
	cp $(BIT_TMP_FILE) $(BIT_FILE)
$(BIT_TMP_FILE): xilinx_spiOverJtag.vhd constr.xdc
	$(VIVADO) xilinx_spiOverJtag.tcl -tclargs $(MODEL)
clean:
	-rm -rf tmp *.jou *.log .Xil
