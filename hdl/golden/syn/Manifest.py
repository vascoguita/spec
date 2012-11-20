target = "xilinx"
action = "synthesis"

fetchto = "../ip_cores"

syn_device = "xc6slx45t"
syn_grade = "-3"
syn_package = "fgg484"
syn_top = "spec_init"
syn_project = "spec_init.xise"

modules = { "local" : 
	[ "../top", 
	  "../platform/xilinx/chipscope" ] 
}
