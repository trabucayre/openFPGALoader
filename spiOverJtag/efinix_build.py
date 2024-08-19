#!/usr/bin/env python3

import argparse
import datetime
import os
import pathlib
import pprint
import re
import sys

from edalize.edatool import get_edatool
from edalize.flows.efinity import Efinity

#from xml.dom import expatbuilder
#import xml.etree.ElementTree as et

#efinity_home = os.environ["EFINITY_HOME"]
#script_path  = efinity_home + "/scripts" 
curr_path = os.getcwd()

efinix_pinout = {
    "Trion": {
        "F49": { # t4/t8
            "ss_n": "G3", "cclk": "F3", "cdi0": "F2", "cdi1": "F1", "cdi2": "E2", "cdi3": "D2",
        },
        "F81": { # t4/t8
            "ss_n": "J4", "cclk": "H4", "cdi0": "F4", "cdi1": "H3", "cdi2": "J2", "cdi3": "F3",
        },
        "F169": { # t13/t20
            "ss_n": "L1", "cclk": "K1", "cdi0": "J1", "cdi1": "J2", "cdi2": "F1", "cdi3": "G2",
        },
        "F256": { # t13/t20
            "ss_n": "P3", "cclk": "H3", "cdi0": "L3", "cdi1": "N1", "cdi2": "K4", "cdi3": "L2",
        },
        "F324": { # t20/t85/t120
            "ss_n": "P15", "cclk": "N13", "cdi0": "M13", "cdi1": "N14", "cdi2": "K14", "cdi3": "K18",
        },
        "F400": { # t20/
            "ss_n": "W18", "cclk": "W19", "cdi0": "Y17", "cdi1": "Y18", "cdi2": "P15", "cdi3": "R17",
        },
        "Q100": { # t13/t20
            "ss_n": "24", "cclk": "26", "cdi0": "19", "cdi1": "18", "cdi2": "8", "cdi3": "14",
        },
        "Q144": { # t20/
            "ss_n": "31", "cclk": "30", "cdi0": "29", "cdi1": "28", "cdi2": "20", "cdi3": "19",
        },
        "W80": { # t20/
            "ss_n": "K3", "cclk": "K2", "cdi0": "J1", "cdi1": "J2", "cdi2": "F1", "cdi3": "G2",
        },
    },
}

def gen_isf_constr(gateware_name, build_path, device_name, family, pkg):

    # Basic settings
    isf_array = [
        "# Device setting",
        "design.set_device_property(\"1A\",\"VOLTAGE\",\"3.3\",\"IOBANK\")",
        "design.set_device_property(\"1B\",\"VOLTAGE\",\"3.3\",\"IOBANK\")",
        "design.set_device_property(\"1C\",\"VOLTAGE\",\"1.1\",\"IOBANK\")",
        "design.set_device_property(\"2A\",\"VOLTAGE\",\"3.3\",\"IOBANK\")",
        "design.set_device_property(\"2B\",\"VOLTAGE\",\"3.3\",\"IOBANK\")",
        "",
    ]

    # JTAG settings
    isf_array.append("# ---------- JTAG 1 ---------")
    isf_array.append("design.create_block(\"jtag_soc\", block_type=\"JTAG\")")
    isf_array.append("design.assign_resource(\"jtag_soc\", \"JTAG_USER1\", \"JTAG\")")
    jtag_pads = [
        "CAPTURE", "DRCK", "RESET", "RUNTEST", "SEL", "SHIFT", "TCK", "TDI", "TMS", "UPDATE", "TDO"
    ]

    for pad in jtag_pads:
        isf_array.append(f"design.set_property(\"jtag_soc\", \"{pad}\", \"jtag_1_{pad}\", \"JTAG\")")

    # SPI pins settings
    pins = efinix_pinout.get(family).get(pkg, None)
    assert pins is not None
    
    pin_lst = [
        {"name" : "csn",     "dir": "out", "pin": pins["ss_n"], "io_std": "3.3 V LVTTL / LVCMOS"},
        {"name" : "sck",     "dir": "out", "pin": pins["cclk"], "io_std": "3.3 V LVTTL / LVCMOS"},
        {"name" : "sdi_dq0", "dir": "out", "pin": pins["cdi0"], "io_std": "3.3 V LVTTL / LVCMOS"},
        {"name" : "sdo_dq1", "dir": "in",  "pin": pins["cdi1"], "io_std": "3.3 V LVTTL / LVCMOS"},
    ]
    
    for pin_cfg in pin_lst:
        name = pin_cfg["name"]
        pin_loc = pin_cfg["pin"]
        if pin_cfg["dir"] == "in":
            isf_array.append(f"design.create_input_gpio(\"{name}\")")
        else:
            isf_array.append(f"design.create_output_gpio(\"{name}\")")
        isf_array.append(f"design.assign_pkg_pin(\"{name}\", \"{pin_loc}\")")
    
    isf_array.append("")

    # Save ISF file
    with open(os.path.join(build_dir, build_name+".isf"), "w") as fd:
        fd.write("\n".join(isf_array))

if __name__ == "__main__":
    parser = argparse.ArgumentParser("SpiOverJtag for Efinix devices")
    parser.add_argument("--device", help="Efinix Device")
    args = parser.parse_args()

    assert args.device is not None

    device        = args.device.upper()
    build_name    = "efinix_spiOverJtag"
    build_dir     = os.path.join(curr_path, f"tmp_efinix_{device.lower()}")
    timing_model  = "C2" # FIXME: always usable (trion / titanium) ?
    sources       = [
        {
            'name': os.path.join(curr_path, "efinix_spiOverJtag.v"),
            "file_type": "verilogSource",
        },
        {
            'name': os.path.join(build_dir, "efinix_spiOverJtag.isf"),
            "file_type": "ISF",
        },
    ]

    force_restart = False

    t = re.compile(r"(T[I]*)(\d+)(\w\d+)")
    tt = t.match(device)
    if tt is None:
        print("fails")
    else:
        (fam, size, package) = tt.groups()
        
        assert fam in ["TI", "T"]

        family = {True:"Titanium", False:"Trion"}[fam == "TI"]
        if fam == "TI":
            device = device.replace("TI", "Ti")

        if os.path.exists(build_dir) and force_restart:
            os.rmdir(build_dir)

        if not os.path.exists(build_dir):
            try:
                os.mkdir(build_dir)
            except FileExistsError:
                pass

        gen_isf_constr(
            gateware_name = build_name,
            build_path    = build_dir,
            device_name   = device,
            family        = family,
            pkg           = package
        )

        tool_options = {
            'part'   : device,
            'family' : family,
            'timing' : timing_model,
        }

        edam = {
            'name'         : build_name,
            'files'        : sources,
            'flow_options' : tool_options,
            'toplevel'     : 'spiOverJtag',
        }
        
        backend = Efinity(edam=edam, work_root=build_dir)
        backend.configure()
        backend.build()

        import shutil
        shutil.copy(os.path.join(build_dir, "outflow", "efinix_spiOverJtag.bit"), build_dir)
