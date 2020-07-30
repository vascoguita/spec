target = "xilinx"
action = "synthesis"
board  = "spec"

syn_device  = "xc6slx45t"
syn_grade   = "-3"
syn_package = "fgg484"
syn_project = "spec_golden-45T.xise"
syn_tool    = "ise"
syn_top     = "spec_golden"

spec_base_ucf = ['onewire', 'spi', 'ddr3']

ctrls = ["bank3_32b_32b" ]

files = [
  "buildinfo_pkg.vhd",
]

modules = {
  "local" : [
    "../../top/golden",
    "../../syn/common",
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
