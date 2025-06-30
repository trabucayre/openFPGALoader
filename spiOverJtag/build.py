#!/usr/bin/env python3

import os

from edalize.edatool import get_edatool

packages = {
    "Artix": {
        "xc7a12t"  : ["cpg238", "csg325"],
        "xc7a15t"  : ["cpg236", "csg324", "csg325", "ftg256", "fgg484"],
        "xc7a25t"  : ["cpg238", "csg325"],
        "xc7a35t"  : ["cpg236", "csg324", "csg325", "ftg256", "fgg484"],
        "xc7a50t"  : ["cpg236", "csg324", "csg325", "ftg256", "fgg484"],
        "xc7a75t"  : ["csg324", "ftg256", "fgg484", "fgg676"],
        "xc7a100t" : ["csg324", "ftg256", "fgg484", "fgg676"],
        "xc7a200t" : ["sbg484", "fbg484", "fbg676", "ffg1156"],
    },
    "Spartan 7": {
        "xc7s6"   : ["ftgb196", "cpga196", "csga225"],
        "xc7s15"  : ["ftgb196", "cpga196", "csga225"],
        "xc7s25"  : ["ftgb196", "csga225", "csga324"],
        "xc7s50"  : ["ftgb196", "csga324", "fgga484"],
        "xc7s75"  : ["fgga484", "fgga676"],
        "xc7s100" : ["fgga484", "fgga676"],
    },
}

if len(os.sys.argv) != 2:
    print("missing board param")
    os.sys.exit()
part = os.sys.argv[1]

build_dir="tmp_" + part
if not os.path.isdir(build_dir):
    try:
        os.mkdir(build_dir)
    except OSError:
        print ("Creation of the directory %s failed" % build_dir)
    else:
        print ("Successfully created the directory %s " % build_dir)

currDir    = os.path.abspath(os.path.curdir) + '/'
files      = []
parameters = {}
pkg_name   = None
model      = ""

subpart = part[0:4].lower()
if subpart == '10cl':
    family = "Cyclone 10 LP"
    tool = "quartus"
elif subpart == 'ep4c':
    family = "Cyclone IV E"
    tool = "quartus"
elif subpart[0:2] == '5c':
    family = "Cyclone V"
    tool = "quartus"
    files.append({'name': currDir + 'constr_cycloneV.tcl',
                  'file_type': 'tclSource'})
elif subpart[0:2] == '5s':
    family = "Stratix V"
    tool = "quartus"
    files.append({'name': currDir + 'constr_cycloneV.tcl',
                  'file_type': 'tclSource'})
elif subpart == "xc7a":
    family = "Artix"
    tool   = "vivado"
    model  = subpart
elif subpart == "xc7v":
    family = "Virtex 7"
    tool = "vivado"
elif subpart == "xc7k":
    device_size = int(part.split('k')[1].split('t')[0])
    if device_size <= 160:
        family = "Kintex 7"
        tool = "vivado"
    else:
        family = "Kintex7"
        tool = "ise"
    speed = -2
elif subpart == "xc7s":
    family = "Spartan 7"
    tool   = "vivado"
    model  = subpart
elif subpart == "xc6s":
    family = "Spartan6"
    tool = "ise"
    speed = -3
elif subpart == "xc3s":
    family = "Spartan3E"
    tool = "ise"
    speed = -4
elif subpart == "xc6v":
    family = "Virtex6"
    tool = "ise"
    speed = -1
elif subpart in ["xcvu", "xcku", "xcau"]:
    family = "Xilinx UltraScale"
    tool = "vivado"
else:
    print("Error: unknown device")
    os.sys.exit()

if model in ["xc7a", "xc7s"]:
    pkg      = packages[family][part][0]
    pkg_name = f"{model}_{pkg}"

if tool in ["ise", "vivado"]:
    pkg_name = {
        "xc3s500evq100"    : "xc3s_vq100",
        "xc6slx9tqg144"    : "xc6s_tqg144",
        "xc6slx9csg324"    : "xc6s_csg324",
        "xc6slx16ftg256"   : "xc6s_ftg256",
        "xc6slx16csg324"   : "xc6s_csg324",
        "xc6slx25csg324"   : "xc6s_csg324",
        "xc6slx25tcsg324"  : "xc6s_t_csg324",
        "xc6slx45csg324"   : "xc6s_csg324",
        "xc6slx45tfgg484"  : "xc6s_t_fgg484",
        "xc6slx100fgg484"  : "xc6s_fgg484",
        "xc6slx150tcsg484" : "xc6s_csg484",
        "xc6slx150tfgg484" : "xc6s_t_fgg484",
        "xc6vlx130tff784"  : "xc6v_ff784",
        "xc7k70tfbg484"    : "xc7k_fbg484",
        "xc7k70tfbg676"    : "xc7k_fbg676",
        "xc7k160tffg676"   : "xc7k_ffg676",
        "xc7k325tffg676"   : "xc7k_ffg676",
        "xc7k325tffg900"   : "xc7k_ffg900",
        "xc7k420tffg901"   : "xc7k_ffg901",
        "xc7vx330tffg1157" : "xc7v_ffg1157",
        "xcku040-ffva1156" : "xcku040_ffva1156",
        "xcku060-ffva1156" : "xcku060_ffva1156",
        "xcvu9p-flga2104"  : "xcvu9p_flga2104",
        "xcvu37p-fsvh2892" : "xcvu37p_fsvh2892",
        "xcku3p-ffva676"   : "xcku3p_ffva676",
        "xcku5p-ffvb676"   : "xcku5p_ffvb676",
        "xcau15p-ffvb676"  : "xcau15p_ffvb676",
    }.get(part, pkg_name)
    if tool == "ise":
        cst_type = "UCF"
        tool_options = {'family': family,
                        'device': {
                            "xc3s500evq100":    "xc3s500e",
                            "xc6slx9tqg144":    "xc6slx9",
                            "xc6slx9csg324":    "xc6slx9",
                            "xc6slx16ftg256":   "xc6slx16",
                            "xc6slx16csg324":   "xc6slx16",
                            "xc6slx25csg324":   "xc6slx25",
                            "xc6slx25tcsg324":  "xc6slx25t",
                            "xc6slx45csg324":   "xc6slx45",
                            "xc6slx45tfgg484":  "xc6slx45t",
                            "xc6slx100fgg484":  "xc6slx100",
                            "xc6slx150tcsg484": "xc6slx150t",
                            "xc6slx150tfgg484": "xc6slx150t",
                            "xc6vlx130tff784":  "xc6vlx130t",
                            "xc7k325tffg676":   "xc7k325t",
                            "xc7k325tffg900":   "xc7k325t",
                            "xc7k420tffg901":   "xc7k420t",
                            }[part],
                        'package': {
                            "xc3s500evq100":    "vq100",
                            "xc6slx9tqg144":    "tqg144",
                            "xc6slx9csg324":    "csg324",
                            "xc6slx16ftg256":   "ftg256",
                            "xc6slx16csg324":   "csg324",
                            "xc6slx25csg324":   "csg324",
                            "xc6slx25tcsg324":  "csg324",
                            "xc6slx45csg324":   "csg324",
                            "xc6slx45tfgg484":  "fgg484",
                            "xc6slx100fgg484":  "fgg484",
                            "xc6slx150tcsg484": "csg484",
                            "xc6slx150tfgg484": "fgg484",
                            "xc6vlx130tff784":  "ff784",
                            "xc7k325tffg676":   "ffg676",
                            "xc7k325tffg900":   "ffg900",
                            "xc7k420tffg901":   "ffg901",
                            }[part],
                        'speed' : speed
                }
    else:
        cst_type = "xdc"
        # Artix/Spartan 7 Specific use case:
        if family in ["Artix", "Spartan 7"]:
            tool_options = {'part': f"{part}{pkg}-1"}
        elif family == "Xilinx UltraScale":
            if part in ["xcvu9p-flga2104", "xcku5p-ffvb676"]:
                tool_options = {'part': part + '-1-e'}
                parameters["secondaryflash"]= {
                    'datatype': 'int',
                    'paramtype': 'vlogdefine',
                    'description': 'secondary flash',
                    'default': 1}
            elif part == "xcku3p-ffva676":
                tool_options = {'part': part + '-2-e'}
            elif part == "xcvu37p-fsvh2892":
                tool_options = {'part': part + '-2L-e'}
            elif part in ["xcku040-ffva1156", "xcku060-ffva1156"]:
                tool_options = {'part': part + '-2-e'}
                parameters["secondaryflash"]= {
                    'datatype': 'int',
                    'paramtype': 'vlogdefine',
                    'description': 'secondary flash',
                    'default': 1}
            elif part == "xcau15p-ffvb676":
                tool_options = {'part': part + '-2-e'}
        else:
            tool_options = {'part': part + '-1'}

    cst_file = currDir + "constr_" + pkg_name + "." + cst_type.lower()
    files.append({'name': currDir + 'xilinx_spiOverJtag.v',
                  'file_type': 'verilogSource'})
    files.append({'name': cst_file, 'file_type': cst_type})
else:
    full_part = {
        "10cl016484" : "10CL016YU484C8G",
        "10cl025256" : "10CL025YU256C8G",
        "10cl055484" : "10CL055YU484C8G",
        "ep4cgx15027": "EP4CGX150DF27I7",
        "ep4ce11523" : "EP4CE115F23C7",
        "ep4ce2217"  : "EP4CE22F17C6",
        "ep4ce1523"  : "EP4CE15F23C8",
        "ep4ce1017"  : "EP4CE10F17C8",
        "ep4ce622"   : "EP4CE6E22C8",
        "5ce215"     : "5CEBA2U15C8",
        "5ce223"     : "5CEFA2F23I7",
        "5ce523"     : "5CEFA5F23I7",
        "5ce423"     : "5CEBA4F23C8",
        "5ce927"     : "5CEBA9F27C7",
        "5cse423"    : "5CSEMA4U23C6",
        "5cse623"    : "5CSEBA6U23I7",
        "5sgsd5"     : "5SGSMD5K2F40I3"}[part]
    files.append({'name': currDir + 'altera_spiOverJtag.v',
                  'file_type': 'verilogSource'})
    files.append({'name': currDir + 'altera_spiOverJtag.sdc',
                  'file_type': 'SDC'})
    tool_options = {'device': full_part, 'family':family}

files.append({'name': currDir + 'spiOverJtag_core.v',
              'file_type': 'verilogSource'})

parameters[family.lower().replace(' ', '')]= {
    'datatype': 'int',
    'paramtype': 'vlogdefine',
    'description': 'fpga family',
    'default': 1}

edam = {'name' : "spiOverJtag",
        'files': files,
        'tool_options': {tool: tool_options},
        'parameters': parameters,
        'toplevel' : 'spiOverJtag',
}

backend = get_edatool(tool)(edam=edam, work_root=build_dir)
backend.configure()
backend.build()

if tool == "vivado":
    import shutil
    import subprocess
    import gzip

    # Compress bitstream.
    with open(f"tmp_{part}/spiOverJtag.bit", 'rb') as bit:
        with gzip.open(f"spiOverJtag_{part}.bit.gz", 'wb', compresslevel=9) as bit_gz:
            shutil.copyfileobj(bit, bit_gz)

    # Create Symbolic links for all supported packages.
    if family in ["Artix", "Spartan 7"]:
        in_file = f"spiOverJtag_{part}.bit.gz"
        for pkg in packages[family][part]:
            out_file = f"spiOverJtag_{part}{pkg}.bit.gz"
            if not os.path.exists(out_file):
                subprocess.run(["ln", "-s", in_file, out_file])
