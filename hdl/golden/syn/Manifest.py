target = "xilinx"
action = "synthesis"

fetchto = "../ip_cores"

syn_device = "xc6slx45t"
syn_grade = "-3"
syn_package = "fgg484"
syn_project = "spec_init.xise"
syn_tool = "ise"
syn_top = "spec_init"

modules = { "local" : "../top",
            "git" : [ "git://ohwr.org/hdl-core-lib/wr-cores.git::proposed_master",
            "git://ohwr.org/hdl-core-lib/general-cores.git::proposed_master",
            "git://ohwr.org/hdl-core-lib/gn4124-core.git::proposed_master" ]
}
