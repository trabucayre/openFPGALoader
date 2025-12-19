"""
openFPGALoader Python bindings

Python wrapper for the openFPGALoader library.
"""

from ._openfpgaloader import (
    load_bitstream,
    detect_fpga,
    list_boards,
    list_cables,
    list_fpgas,
    OpenFPGALoader,
)

__version__ = "1.0.0"

__all__ = [
    "load_bitstream",
    "detect_fpga",
    "list_boards",
    "list_cables",
    "list_fpgas",
    "OpenFPGALoader",
]
