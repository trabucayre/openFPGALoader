# openFPGALoader Python Bindings

Python bindings for openFPGALoader - Universal utility for programming FPGAs.

## Installation

```bash
pip install openfpgaloader
```

## Usage

```python
import openfpgaloader as ofl

# List supported boards
boards = ofl.list_boards()
print(f"Supported boards: {boards[:5]}")

# List supported cables  
cables = ofl.list_cables()
print(f"Supported cables: {cables[:5]}")

# Detect connected FPGA
detected = ofl.detect_fpga()

# Load bitstream to SRAM
ofl.load_bitstream("my_design.bit", board="arty")

# Load bitstream to Flash
ofl.load_bitstream("my_design.bit", board="arty", to_flash=True)

# Using the class interface
loader = ofl.OpenFPGALoader(board="arty", verbose=1)
loader.program_sram("my_design.bit")
loader.program_flash("my_design.bit", offset=0)
```

## Building from Source

```bash
# Using pixi (recommended)
pixi run build-wheel

# Or using pip
pip install .
```

## License

Apache-2.0
