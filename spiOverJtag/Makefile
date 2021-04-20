VIVADO := vivado -nolog -nojournal -mode batch -source
PARTS := xc7a35tcpg236 xc7a35tcsg324 xc7a35tftg256 xc7a50tcpg236 xc7a75tfgg484 xc7a100tfgg484 xc7a200tsbg484 xc7s50csga324
BIT_FILES := $(addsuffix .bit,$(addprefix spiOverJtag_, $(PARTS)))

all: $(BIT_FILES)

$(BIT_FILES) : spiOverJtag_%.bit : tmp_%/spiOverJtag.runs/impl_1/xilinx_spiOverJtag.bit
	cp $< $@
tmp_%/spiOverJtag.runs/impl_1/xilinx_spiOverJtag.bit : xilinx_spiOverJtag.vhd
	$(VIVADO) xilinx_spiOverJtag.tcl -tclargs $*

clean:
	-rm -rf tmp_* *.jou *.log .Xil
