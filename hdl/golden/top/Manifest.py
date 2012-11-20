files = ["spec_init.vhd", "spec_init.ucf"]

fetchto = "../ip_cores"

modules = {
    "local" : ["../"],
    "svn" : [ "http://svn.ohwr.org/gn4124-core/trunk/hdl/gn4124core/rtl" ]
    }
