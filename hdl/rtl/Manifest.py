# SPDX-FileCopyrightText: 2020 CERN (home.cern)
#
# SPDX-License-Identifier: CERN-OHL-W-2.0+

files = [
    "spec_base_regs.vhd",
    "spec_base_wr.vhd",
    "sourceid_spec_base_pkg.vhd",
]

try:
    # Assume this module is in fact a git submodule of a main project that
    # is in the same directory as general-cores...
    exec(open("../../../" + "general-cores/tools/gen_sourceid.py").read(),
         None, {'project': 'spec_base'})
except Exception as e:
    try:
        # Otherwise look for the local submodule of general-cores
        exec(open("../ip_cores/" + "general-cores/tools/gen_sourceid.py").read(),
             None, {'project': 'spec_base'})

    except Exception as e:
        print("Error: cannot generate source id file")
        raise
