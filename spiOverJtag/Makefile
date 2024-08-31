XILINX_PARTS := xc3s500evq100 \
	xc6slx9tqg144 xc6slx9csg324 \
	xc6slx16ftg256 xc6slx16csg324 xc6slx25csg324 xc6slx45csg324 xc6slx100fgg484 \
	xc6slx25tcsg324 xc6slx45tfgg484 xc6slx150tfgg484 xc6slx150tcsg484 \
	xc6vlx130tff784 \
	xc7a15tcpg236 \
	xc7a25tcpg238 xc7a25tcsg325 \
	xc7a35tcpg236 xc7a35tcsg324 xc7a35tftg256 xc7a35tfgg484 \
	xc7a50tcsg324 xc7a50tfgg484 xc7a50tcpg236 xc7a75tfgg484 \
	xc7a100tcsg324 xc7a100tfgg484 xc7a100tfgg676\
	xc7a200tsbg484 xc7a200tfbg484 xc7a200tfbg676\
	xc7s6ftgb196 xc7s25csga225 xc7s25csga324 xc7s50csga324 \
	xc7k70tfbg484 xc7k70tfbg676 \
	xc7k160tffg676 \
	xc7k325tffg676 xc7k325tffg900 \
	xc7k420tffg901 \
	xcku3p-ffva676 \
	xc7vx330tffg1157 \
	xcku5p-ffvb676 \
	xcvu9p-flga2104 xcvu37p-fsvh2892
XILINX_BIT_FILES := $(addsuffix .bit.gz,$(addprefix spiOverJtag_, $(XILINX_PARTS)))

ALTERA_PARTS := 10cl025256 10cl016484 10cl055484 \
	ep4ce622 ep4ce1017 ep4ce2217 ep4ce1523 ep4ce11523 ep4cgx15027 5ce215 5ce223 5ce423 5ce523 5ce927
ALTERA_BIT_FILES := $(addsuffix .rbf.gz, $(addprefix spiOverJtag_, $(ALTERA_PARTS)))

EFINIX_PARTS := t8f81 t13f256 ti180j484
EFINIX_BIT_FILES := $(addsuffix .bit.gz, $(addprefix spiOverJtag_efinix_, $(EFINIX_PARTS)))

BIT_FILES := $(ALTERA_BIT_FILES) $(EFINIX_BIT_FILES) $(XILINX_BIT_FILES)

all: $(BIT_FILES)

$(EFINIX_BIT_FILES) : spiOverJtag_efinix_%.bit.gz : tmp_efinix_%/efinix_spiOverJtag.bit
	gzip -9 -c $< > $@
tmp_efinix_%/efinix_spiOverJtag.bit : efinix_spiOverJtag.v
	./efinix_build.py --device $*

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
