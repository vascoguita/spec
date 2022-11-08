# SPDX-FileCopyrightText: 2020 CERN (home.cern)
#
# SPDX-License-Identifier: CERN-OHL-W-2.0+

modules = { "local" : [ "hdl/rtl" ] }

if action == "synthesis":
    modules["local"].append("hdl/syn/common")
