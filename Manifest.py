modules = { "local" : [ "hdl/rtl" ] }

if action == "synthesis":
    modules["local"].append("hdl/syn/common")
