VIVADO := vivado -nolog -nojournal -mode batch -source
MODELS := xc7a35 xc7a50t xc7a100 xc7s50 xc7a200
BIT_FILES := $(addsuffix .bit,$(addprefix spiOverJtag_, $(MODELS)))

all: $(BIT_FILES)

$(BIT_FILES) : spiOverJtag_%.bit : tmp_%/spiOverJtag.runs/impl_1/xilinx_spiOverJtag.bit
	cp $< $@
tmp_%/spiOverJtag.runs/impl_1/xilinx_spiOverJtag.bit : xilinx_spiOverJtag.vhd constr_%.xdc
	$(VIVADO) xilinx_spiOverJtag.tcl -tclargs $*

clean:
	-rm -rf tmp_* *.jou *.log .Xil
