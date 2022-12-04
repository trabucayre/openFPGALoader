XILINX_PARTS := xc3s500evq100 xc6slx9tqg144 xc6slx16ftg256 xc6slx16csg324 xc6slx45csg324 xc6slx100fgg484 \
	xc6slx150tfgg484 \
	xc7a35tcpg236 xc7a35tcsg324 xc7a35tftg256 \
	xc7a50tcsg324 xc7a50tcpg236 xc7a75tfgg484 \
	xc7a100tcsg324 xc7a100tfgg484 xc7a100tfgg676\
	xc7a200tsbg484 \
	xc7s25csga225 xc7s25csga324 xc7s50csga324 \
	xc7k160tffg676 \
	xc7k325tffg676 xc7k325tffg900 \
	xc7k420tffg901
XILINX_BIT_FILES := $(addsuffix .bit.gz,$(addprefix spiOverJtag_, $(XILINX_PARTS)))

ALTERA_PARTS := 10cl025256 ep4ce2217 ep4ce1523 ep4ce11523 5ce223 5ce423 5ce523 5ce927
ALTERA_BIT_FILES := $(addsuffix .rbf.gz, $(addprefix spiOverJtag_, $(ALTERA_PARTS)))

BIT_FILES := $(ALTERA_BIT_FILES) $(XILINX_BIT_FILES)

all: $(BIT_FILES)

$(XILINX_BIT_FILES) : spiOverJtag_%.bit.gz : tmp_%/spiOverJtag.bit
	gzip -9 -c $< > $@
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
