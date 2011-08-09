files = ["spec_ddr_test.vhd",
         "gpio_regs.vhd"]

modules = {"svn" : ["http://svn.ohwr.org/gn4124-core/trunk/hdl/gn4124core/rtl",
                    "http://svn.ohwr.org/gn4124-core/trunk/hdl/common/rtl",
                    "http://svn.ohwr.org/ddr3-sp6-core/trunk/hdl"],
           "git" : "git://ohwr.org/hdl-core-lib/general-cores.git"}

fetchto = "../ip_cores"
