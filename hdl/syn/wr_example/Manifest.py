target = "xilinx"
action = "synthesis"
board  = "spec"

syn_device  = "xc6slx45t"
syn_grade   = "-3"
syn_package = "fgg484"
syn_project = "spec_base_wr_example.xise"
syn_tool    = "ise"
syn_top     = "spec_base_wr_example"

spec_base_ucf = ['wr', 'onewire', 'spi', 'ddr3']

ctrls = ["bank3_32b_32b" ]

files = [ "buildinfo_pkg.vhd" ]

modules = {
  "local" : [
    "../../top/wr_example",
    "../../syn/common",
  ],
  "git" : [
      "https://ohwr.org/project/wr-cores.git",
      "https://ohwr.org/project/general-cores.git",
      "https://ohwr.org/project/gn4124-core.git",
      "https://ohwr.org/project/ddr3-sp6-core.git",
  ],
}

# Allow the user to override fetchto using:
#  hdlmake -p "fetchto='xxx'"
if locals().get('fetchto', None) is None:
  fetchto = "../../ip_cores"

# Do not fail during hdlmake fetch
try:
  exec(open(fetchto + "/general-cores/tools/gen_buildinfo.py").read())
except:
  pass

syn_post_project_cmd = "$(TCL_INTERPRETER) syn_extra_steps.tcl $(PROJECT_FILE)"
