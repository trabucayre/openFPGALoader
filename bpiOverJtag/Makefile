BPI_XILINX_PARTS     := xc7k480tffg1156
BPI_XILINX_BIT_FILES := $(addsuffix .bit.gz,$(addprefix bpiOverJtag_, $(BPI_XILINX_PARTS)))

BIT_FILES := $(BPI_XILINX_BIT_FILES)

all: $(BIT_FILES)

$(BPI_XILINX_BIT_FILES) : bpiOverJtag_%.bit.gz : tmp_%/bpiOverJtag.bit

tmp_%/bpiOverJtag.bit : xilinx_bpiOverJtag.v bpiOverJtag_core.v
	../spiOverJtag/build.py $* bpi

clean:
	-rm -rf tmp_* *.jou *.log .Xil
