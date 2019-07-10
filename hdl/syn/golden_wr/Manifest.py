target = "xilinx"
action = "synthesis"

fetchto = "../ip_cores"

syn_device = "xc6slx45t"
syn_grade = "-3"
syn_package = "fgg484"
syn_project = "spec_golden_wr.xise"
syn_tool = "ise"
syn_top = "spec_golden_wr"
syn_properties = [ ["-generics", "dpram=\"3\""]]

board = "spec"
ctrls = ["bank3_64b_32b" ]

modules = { 
  "local" : "../../top/golden_wr",
  "git" : [ "https://ohwr.org/project/wr-cores.git::proposed_master",
            "https://ohwr.org/project/general-cores.git::proposed_master",
            "https://ohwr.org/project/etherbone-core.git::proposed_master",
            "https://ohwr.org/project/gn4124-core.git::proposed_master",
            "https://ohwr.org/project/ddr3-sp6-core.git::proposed_master" ]
}
