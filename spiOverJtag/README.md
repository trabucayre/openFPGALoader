# spiOverJtag

*Lattice ECP3/ECP5*, *Gowin GW2/GW5*, and *Cologne Chip GateMate*
have an internal interface to access external SPI flash via *JTAG*.
In contrast, accessing SPI flash on *Xilinx*, *Efinix* and
*Altera Cyclone* FPGAs requires loading a dedicated bitstream that
bridges *JTAG* and *SPI* interfaces.

`spiOverJtag` contains bridge bitstreams used by `openFPGALoader` to access
external SPI flash over JTAG on supported Xilinx, Altera, and Efinix devices.

>## Important notes
>
>- *spiOverJtag* supports Single-Wire mode only (`MOSI`/`MISO`) for maximum
>  board compatibility.
>- Existing bridge bitstreams are versioned in the repository. Rebuild is
>  typically required only when adding support for a missing FPGA model/package.

## Dependencies

- `make`
- `gzip`
- `python3`
- Python package: `edalize`
- Vendor toolchains (depending on target family):
  - Xilinx: Vivado and/or ISE
  - Altera: Quartus
  - Efinix: Efinity

Notes:
- `build.py` is used for **Xilinx** and **Altera** targets.
- `efinix_build.py` is used for **Efinix** targets.

## Build generalities

All steps mentioned in next sections must be performed from `spiOverJtag`
sub-directory.

Generated outputs:

- *Xilinx*: `spiOverJtag_<part>.bit.gz`
- *Efinix*: `spiOverJtag_efinix_<part>.bit.gz`
- *Altera*: `spiOverJtag_<part>.rbf.gz`

Build command examples:

```bash
# Xilinx Artix7 35t
make spiOverJtag_xc7a35t.bit.gz
# Altera Cyclone10CL 016
make spiOverJtag_10cl016484.rbf.gz
# Efinix Trion T13 F256
make spiOverJtag_efinix_t13f256.bit.gz
```

Clean temporary/build files:

```bash
make clean
```

## Add support for a new device

### 1. Register the part in `Makefile`

Add the short part name into the appropriate list:
- `XILINX_PARTS`
- `ALTERA_PARTS`
- `EFINIX_PARTS`

### 2. Update build script mappings

For *Xilinx* (`build.py`):

Approach for *Xilinx* FPGAs depends on family:
- for `Artix`, `Kintex 7` and `Spartan 7` size and packages are provided via
  `packages` dict.
- for others devices `pkg_name` (*Vivado*) or `tool_options` (*ISE*) must be
  updated to add the relationship between FPGA model/size/package and
  device/package.

For *Altera* (`build.py`):

The `full_part` dict must be updated to match the short format and a
*Quartus*-compatible device name.

For *Efinix* (`efinix_build.py`):

The script contains pin mapping data for *Trion* and *Titanium* devices.

The principle is to update `efinix_pinout` with a new sub-dict that provides
the pin name for each SPI IO.

The `timing_models` dict must also be updated to add an entry for the new FPGA.

> Note:
> For *Titanium* devices, ensure the package mapping exists in
> `efinix_pinout["Titanium"]` and the corresponding timing model exists in
> `timing_models`.

### 3. Add constraints (if required)

*spiOverJtag* already contains many constraint files.
Usually, devices in the same family with the same package share the same
JTAG and SPI pinout (for example, *Xilinx Artix7* devices with *ftg256*
can reuse `constr_xc7a_ftg256`, regardless of device size).
This step is only required when a package is not yet available for a given
FPGA family.

Add the constraint file expected by the mapping:

- *Xilinx Vivado*: `constr_<name>.xdc`
- *Xilinx ISE*: `constr_<name>.ucf`
- *Altera*: no explicit constraint file in this directory (handled by Quartus
  and script flow)
- *Efinix*: generated/handled by `efinix_build.py`; update script data for new
  package pinouts when needed

### 4. Build spiOverJtag bitstream

Run the target build command and verify the compressed file is generated:

```bash
# Xilinx/Efinix
make spiOverJtag_<new-part>.bit.gz
# or for Altera
make spiOverJtag_<new-part>.rbf.gz
```

At the end of the tool execution, a new bitstream should be present in the
current directory.

> Note:
>The directory `tmp_<part>/` (or `tmp_efinix_<part>/`) is the working
>directory and may be removed after bitstream generation.
