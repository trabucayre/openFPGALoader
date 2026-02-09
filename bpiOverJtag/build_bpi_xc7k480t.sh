#!/bin/bash
# Build bpiOverJtag bitstream for xc7k480tffg1156

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "Building bpiOverJtag for xc7k480tffg1156..."

# Find Vivado
if ! command -v vivado &> /dev/null; then
    echo "Error: vivado not found in PATH"
    echo "Source Vivado settings first:"
    echo "  source /tools/Xilinx/2025.1/Vivado/settings64.sh"
    exit 1
fi

VIVADO="vivado"

# Run Vivado in batch mode
$VIVADO -mode batch -source build_bpi_xc7k480t.tcl

# Compress the bitstream
if [ -f output_bpi_xc7k480t/bpiOverJtag_xc7k480tffg1156.bit ]; then
    echo "Compressing bitstream..."
    gzip -9 -c output_bpi_xc7k480t/bpiOverJtag_xc7k480tffg1156.bit > bpiOverJtag_xc7k480tffg1156.bit.gz
    echo ""
    echo "Success! Bitstream created: bpiOverJtag_xc7k480tffg1156.bit.gz"
    echo ""
    echo "To install system-wide, copy to the openFPGALoader data directory:"
    echo "  sudo cp bpiOverJtag_xc7k480tffg1156.bit.gz /usr/local/share/openFPGALoader/"
    echo ""
    echo "Or set OPENFPGALOADER_SOJ_DIR to this directory:"
    echo "  export OPENFPGALOADER_SOJ_DIR=$SCRIPT_DIR"
else
    echo "Error: Bitstream generation failed"
    exit 1
fi
