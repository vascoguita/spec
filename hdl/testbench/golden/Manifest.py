# SPDX-FileCopyrightText: 2020 CERN (home.cern)
#
# SPDX-License-Identifier: CC0-1.0

board      = "spec"
sim_tool   = "modelsim"
sim_top    = "main"
action     = "simulation"
target     = "xilinx"
syn_device = "xc6slx45t"

vcom_opt = "-93 -mixedsvvh"

# Allow the user to override fetchto using:
#  hdlmake -p "fetchto='xxx'"
if locals().get('fetchto', None) is None:
    fetchto="../../ip_cores"

include_dirs = [
    fetchto + "/gn4124-core/hdl/sim/gn4124_bfm",
    fetchto + "/general-cores/sim/",
    fetchto + "/ddr3-sp6-core/hdl/sim/",
]

files = [
    "main.sv",
    "buildinfo_pkg.vhd",
]

modules = {
    "local" : [
        "../../top/golden",
    ],
}

# Do not fail during hdlmake fetch
try:
  exec(open(fetchto + "/general-cores/tools/gen_buildinfo.py").read())
except:
  pass

ctrls = ["bank3_32b_32b" ]
