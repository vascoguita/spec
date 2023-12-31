# SPDX-FileCopyrightText: 2020 CERN (home.cern)
#
# SPDX-License-Identifier: CERN-OHL-W-2.0+

vsim -quiet -t 10fs -L unisim work.main -suppress 1270,8617,8683,8684,8822 -sv_seed random
set StdArithNoWarnings 1
set NumericStdNoWarnings 1

run -all
