XILINX_PARTS := xc6slx45csg324 xc6slx100fgg484 \
	xc7a35tcpg236 xc7a35tcsg324 xc7a35tftg256 \
	xc7a50tcpg236 xc7a75tfgg484 xc7a100tfgg484 xc7a200tsbg484 \
	xc7s50csga324
XILINX_BIT_FILES := $(addsuffix .bit,$(addprefix spiOverJtag_, $(XILINX_PARTS)))

ALTERA_PARTS := 10cl025256 ep4ce2217 5ce223 5ce423
ALTERA_BIT_FILES := $(addsuffix .rbf.gz, $(addprefix spiOverJtag_, $(ALTERA_PARTS)))

BIT_FILES := $(ALTERA_BIT_FILES) $(XILINX_BIT_FILES)

all: $(BIT_FILES)

$(XILINX_BIT_FILES) : spiOverJtag_%.bit : tmp_%/spiOverJtag.bit
	cp $< $@
tmp_%/spiOverJtag.bit : xilinx_spiOverJtag.v
	./build.py $*

$(ALTERA_BIT_FILES): spiOverJtag_%.rbf.gz: tmp_%/spiOverJtag.rbf
	gzip -9 -c $< > $@
tmp_%/spiOverJtag.rbf: tmp_%/spiOverJtag.sof
	quartus_cpf --option=bitstream_compression=off -c $< $@

tmp_%/spiOverJtag.sof: altera_spiOverJtag.v
	./build.py $*

clean:
	-rm -rf tmp_* *.jou *.log .Xil
