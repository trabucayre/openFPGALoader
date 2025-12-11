XILINX_PARTS := xc3s500evq100 \
	xc6slx9tqg144 xc6slx9csg324 \
	xc6slx16ftg256 xc6slx16csg324 xc6slx25csg324 xc6slx45csg324 xc6slx100fgg484 \
	xc6slx25tcsg324 xc6slx45tfgg484 xc6slx150tfgg484 xc6slx150tcsg484 \
	xc6vlx130tff784 \
	xc7a12t xc7a15t xc7a25t xc7a35t xc7a50t xc7a75t xc7a100t xc7a200t \
	xc7s6 xc7s15 xc7s25 xc7s50 xc7s75 xc7s100 \
	xc7k70tfbg484 xc7k70tfbg676 \
	xc7k160tffg676 \
	xc7k325tffg676 xc7k325tffg900 \
	xc7k420tffg901 \
	xcku3p-ffva676 \
	xc7vx330tffg1157 \
	xcku040-ffva1156 xcku060-ffva1156 \
	xcku5p-ffvb676 \
	xcvu9p-flga2104 xcvu37p-fsvh2892 \
	xcau15p-ffvb676
XILINX_BIT_FILES := $(addsuffix .bit.gz,$(addprefix spiOverJtag_, $(XILINX_PARTS)))

ALTERA_PARTS := 10cl025256 10cl016484 10cl055484 \
	ep4ce622 ep4ce1017 ep4ce2217 ep4ce1523 ep4ce11523 ep4cgx15027 5ce215 5ce223 5ce423 5ce523 5ce927 5sgsd5
ALTERA_BIT_FILES := $(addsuffix .rbf.gz, $(addprefix spiOverJtag_, $(ALTERA_PARTS)))

EFINIX_PARTS := t8f81 t13f256 t120f324 ti180j484
EFINIX_BIT_FILES := $(addsuffix .bit.gz, $(addprefix spiOverJtag_efinix_, $(EFINIX_PARTS)))

BIT_FILES := $(ALTERA_BIT_FILES) $(EFINIX_BIT_FILES) $(XILINX_BIT_FILES)

all: $(BIT_FILES)

$(EFINIX_BIT_FILES) : spiOverJtag_efinix_%.bit.gz : tmp_efinix_%/efinix_spiOverJtag.bit
	gzip -9 -c $< > $@
tmp_efinix_%/efinix_spiOverJtag.bit : efinix_spiOverJtag.v
	./efinix_build.py --device $*

$(XILINX_BIT_FILES) : spiOverJtag_%.bit.gz : tmp_%/spiOverJtag.bit

tmp_%/spiOverJtag.bit : xilinx_spiOverJtag.v spiOverJtag_core.v
	./build.py $*

$(ALTERA_BIT_FILES): spiOverJtag_%.rbf.gz: tmp_%/spiOverJtag.rbf
	gzip -9 -c $< > $@
tmp_%/spiOverJtag.rbf: tmp_%/spiOverJtag.sof
	quartus_cpf --option=bitstream_compression=off -c $< $@

tmp_%/spiOverJtag.sof: altera_spiOverJtag.v
	./build.py $*

clean:
	-rm -rf tmp_* *.jou *.log .Xil
