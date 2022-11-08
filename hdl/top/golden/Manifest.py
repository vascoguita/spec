# SPDX-FileCopyrightText: 2020 CERN (home.cern)
#
# SPDX-License-Identifier: CERN-OHL-W-2.0+

# Allow the user to override fetchto using:
#  hdlmake -p "fetchto='xxx'"
if locals().get('fetchto', None) is None:
  fetchto = "../../ip_cores"

files = [
    "spec_golden.vhd",
]

modules = {
  "local" : [
    "../../rtl",
  ],
  "git" : [
      "https://ohwr.org/project/wr-cores.git",
      "https://ohwr.org/project/general-cores.git",
      "https://ohwr.org/project/gn4124-core.git",
      "https://ohwr.org/project/ddr3-sp6-core.git",
  ],
}
