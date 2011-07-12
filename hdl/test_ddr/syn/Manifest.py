target = "xilinx"
action = "synthesis"

syn_device = "xc6slx45t"
syn_grade = "-3"
syn_package = "fgg484"
syn_top = "spec_ddr_test"
syn_project = "spec_ddr_test.xise"

files = ["../spec_ddr_test.ucf"]

modules = { "local" : "../rtl" }
