target = "xilinx"
action = "synthesis"

# Allow the user to override fetchto using:
#  hdlmake -p "fetchto='xxx'"
if locals().get('fetchto', None) is None:
  fetchto = "../ip_cores"

syn_device = "xc6slx45t"
syn_grade = "-3"
syn_package = "fgg484"
syn_project = "spec_golden_wr.xise"
syn_tool = "ise"
syn_top = "spec_golden_wr"

board = "spec"
ctrls = ["bank3_64b_32b" ]

files = [ "buildinfo_pkg.vhd" ]

modules = { 
  "local" : "../../top/golden_wr",
  "git" : [ "https://ohwr.org/project/wr-cores.git::proposed_master",
            "https://ohwr.org/project/general-cores.git::proposed_master",
            "https://ohwr.org/project/etherbone-core.git::proposed_master",
            "https://ohwr.org/project/gn4124-core.git::proposed_master",
            "https://ohwr.org/project/ddr3-sp6-core.git::proposed_master" ]
}

# Do not fail during hdlmake fetch
try:
  exec(open(fetchto + "/general-cores/tools/gen_buildinfo.py").read())
except:
  pass
