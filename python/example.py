#!/usr/bin/env python3
"""
Example usage of openFPGALoader Python bindings
"""

import openfpgaloader as ofl


def main():
    print("openFPGALoader Python Bindings Example")
    print(f"Version: {ofl.__version__}\n")

    # List supported boards
    boards = ofl.list_boards()
    print(f"Total supported boards: {len(boards)}")
    print(f"First 10 boards: {boards[:10]}\n")

    # List supported cables
    cables = ofl.list_cables()
    print(f"Total supported cables: {len(cables)}")
    print(f"First 10 cables: {cables[:10]}\n")

    # List supported FPGAs
    fpgas = ofl.list_fpgas()
    print(f"Total supported FPGAs: {len(fpgas)}")
    print(f"First 10 FPGAs: {fpgas[:10]}\n")

    # Create an OpenFPGALoader instance
    print("Creating OpenFPGALoader instance for Arty board...")
    loader = ofl.OpenFPGALoader(board="arty", verbose=0)
    print("Instance created successfully!\n")

    # Example of how to use the loader (would need actual bitstream file)
    # loader.program_sram("my_design.bit")
    # loader.program_flash("my_design.bit", offset=0)

    # Using convenience function
    # ofl.load_bitstream("my_design.bit", board="arty", to_flash=False)

    # ofl.load_bitstream("my_design.bit", board="arty", to_flash=False)

if __name__ == "__main__":
    main()
