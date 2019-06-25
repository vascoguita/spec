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

modules = { 
  "local" : "../top",
  "git" : [ "git://ohwr.org/hdl-core-lib/wr-cores.git::proposed_master",
            "git://ohwr.org/hdl-core-lib/general-cores.git::proposed_master",
            "git://ohwr.org/hdl-core-lib/etherbone-core.git::proposed_master",
            "git://ohwr.org/hdl-core-lib/gn4124-core.git::proposed_master" ]
}
