target = "xilinx"
action = "synthesis"

fetchto = "../../ip_cores"

syn_device = "xc6slx150t"
syn_grade = "-3"
syn_package = "fgg484"
syn_project = "spec_golden_wr.xise"
syn_tool = "ise"
syn_top = "spec_golden_wr"

board = "spec"
ctrls = ["bank3_64b_32b" ]

modules = {
  "local" : [
      "../../top/golden_wr",
      ],
  "git" : [
      "https://ohwr.org/project/wr-cores.git",
      "https://ohwr.org/project/general-cores.git",
      "https://ohwr.org/project/gn4124-core.git",
      "https://ohwr.org/project/ddr3-sp6-core.git",
  ],
}

syn_post_project_cmd = (
    "$(TCL_INTERPRETER) " + \
    fetchto + "/general-cores/tools/sdb_desc_gen.tcl " + \
    syn_tool + " $(PROJECT_FILE);" \
    "$(TCL_INTERPRETER) syn_extra_steps.tcl $(PROJECT_FILE)"
)
