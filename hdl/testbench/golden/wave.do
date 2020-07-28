onerror {resume}
quietly WaveActivateNextPane {} 0
add wave -noupdate -group {App DDR port} -color Coral /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/rst_n_i
add wave -noupdate -group {App DDR port} -group Command -color Gold /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/ddr_cmd_clk_o
add wave -noupdate -group {App DDR port} -group Command -color Gold /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/ddr_cmd_en_o
add wave -noupdate -group {App DDR port} -group Command -color Gold /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/ddr_cmd_instr_o
add wave -noupdate -group {App DDR port} -group Command -color Gold /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/ddr_cmd_bl_o
add wave -noupdate -group {App DDR port} -group Command -color Gold /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/ddr_cmd_byte_addr_o
add wave -noupdate -group {App DDR port} -group Command -color Gold /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/ddr_cmd_empty_i
add wave -noupdate -group {App DDR port} -group Command -color Gold /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/ddr_cmd_full_i
add wave -noupdate -group {App DDR port} -group Write -color Magenta /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/ddr_wr_clk_o
add wave -noupdate -group {App DDR port} -group Write -color Magenta /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/ddr_wr_en_o
add wave -noupdate -group {App DDR port} -group Write -color Magenta /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/ddr_wr_mask_o
add wave -noupdate -group {App DDR port} -group Write -color Magenta /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/ddr_wr_data_o
add wave -noupdate -group {App DDR port} -group Write -color Magenta /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/ddr_wr_full_i
add wave -noupdate -group {App DDR port} -group Write -color Magenta /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/ddr_wr_empty_i
add wave -noupdate -group {App DDR port} -group Write -color Magenta /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/ddr_wr_count_i
add wave -noupdate -group {App DDR port} -group Write -color Magenta /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/ddr_wr_underrun_i
add wave -noupdate -group {App DDR port} -group Write -color Magenta /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/ddr_wr_error_i
add wave -noupdate -group {App DDR port} -group Read -color Cyan /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/ddr_rd_clk_o
add wave -noupdate -group {App DDR port} -group Read -color Cyan /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/ddr_rd_en_o
add wave -noupdate -group {App DDR port} -group Read -color Cyan /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/ddr_rd_data_i
add wave -noupdate -group {App DDR port} -group Read -color Cyan /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/ddr_rd_full_i
add wave -noupdate -group {App DDR port} -group Read -color Cyan /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/ddr_rd_empty_i
add wave -noupdate -group {App DDR port} -group Read -color Cyan /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/ddr_rd_count_i
add wave -noupdate -group {App DDR port} -group Read -color Cyan /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/ddr_rd_overflow_i
add wave -noupdate -group {App DDR port} -group Read -color Cyan /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/ddr_rd_error_i
add wave -noupdate -group {App DDR port} -group WB /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/wb_clk_i
add wave -noupdate -group {App DDR port} -group WB /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/wb_sel_i
add wave -noupdate -group {App DDR port} -group WB /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/wb_cyc_i
add wave -noupdate -group {App DDR port} -group WB /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/wb_stb_i
add wave -noupdate -group {App DDR port} -group WB /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/wb_we_i
add wave -noupdate -group {App DDR port} -group WB /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/wb_addr_i
add wave -noupdate -group {App DDR port} -group WB /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/wb_data_i
add wave -noupdate -group {App DDR port} -group WB /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/wb_data_o
add wave -noupdate -group {App DDR port} -group WB /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/wb_ack_o
add wave -noupdate -group {App DDR port} -group WB /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_0/wb_stall_o
add wave -noupdate -group {Host DDR port} -color Coral /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/rst_n_i
add wave -noupdate -group {Host DDR port} -group Command -color Gold /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/ddr_cmd_clk_o
add wave -noupdate -group {Host DDR port} -group Command -color Gold /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/ddr_cmd_en_o
add wave -noupdate -group {Host DDR port} -group Command -color Gold /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/ddr_cmd_instr_o
add wave -noupdate -group {Host DDR port} -group Command -color Gold /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/ddr_cmd_bl_o
add wave -noupdate -group {Host DDR port} -group Command -color Gold /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/ddr_cmd_byte_addr_o
add wave -noupdate -group {Host DDR port} -group Command -color Gold /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/ddr_cmd_empty_i
add wave -noupdate -group {Host DDR port} -group Command -color Gold /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/ddr_cmd_full_i
add wave -noupdate -group {Host DDR port} -group Write -color Magenta /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/ddr_wr_clk_o
add wave -noupdate -group {Host DDR port} -group Write -color Magenta /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/ddr_wr_en_o
add wave -noupdate -group {Host DDR port} -group Write -color Magenta /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/ddr_wr_mask_o
add wave -noupdate -group {Host DDR port} -group Write -color Magenta /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/ddr_wr_data_o
add wave -noupdate -group {Host DDR port} -group Write -color Magenta /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/ddr_wr_full_i
add wave -noupdate -group {Host DDR port} -group Write -color Magenta /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/ddr_wr_empty_i
add wave -noupdate -group {Host DDR port} -group Write -color Magenta /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/ddr_wr_count_i
add wave -noupdate -group {Host DDR port} -group Write -color Magenta /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/ddr_wr_underrun_i
add wave -noupdate -group {Host DDR port} -group Write -color Magenta /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/ddr_wr_error_i
add wave -noupdate -group {Host DDR port} -group Read -color Cyan /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/ddr_rd_clk_o
add wave -noupdate -group {Host DDR port} -group Read -color Cyan /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/ddr_rd_en_o
add wave -noupdate -group {Host DDR port} -group Read -color Cyan /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/ddr_rd_data_i
add wave -noupdate -group {Host DDR port} -group Read -color Cyan /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/ddr_rd_full_i
add wave -noupdate -group {Host DDR port} -group Read -color Cyan /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/ddr_rd_empty_i
add wave -noupdate -group {Host DDR port} -group Read -color Cyan /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/ddr_rd_count_i
add wave -noupdate -group {Host DDR port} -group Read -color Cyan /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/ddr_rd_overflow_i
add wave -noupdate -group {Host DDR port} -group Read -color Cyan /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/ddr_rd_error_i
add wave -noupdate -group {Host DDR port} -group WB /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/wb_clk_i
add wave -noupdate -group {Host DDR port} -group WB /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/wb_sel_i
add wave -noupdate -group {Host DDR port} -group WB /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/wb_cyc_i
add wave -noupdate -group {Host DDR port} -group WB /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/wb_stb_i
add wave -noupdate -group {Host DDR port} -group WB /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/wb_we_i
add wave -noupdate -group {Host DDR port} -group WB /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/wb_addr_i
add wave -noupdate -group {Host DDR port} -group WB /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/wb_data_i
add wave -noupdate -group {Host DDR port} -group WB /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/wb_data_o
add wave -noupdate -group {Host DDR port} -group WB /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/wb_ack_o
add wave -noupdate -group {Host DDR port} -group WB /main/DUT/inst_spec_base/gen_with_ddr/cmp_ddr_ctrl_bank3/cmp_ddr3_ctrl_wb_1/wb_stall_o
add wave -noupdate -group {P2L DATA} -color {Blue Violet} /main/DUT/inst_spec_base/gen_with_gennum/cmp_gn4124_core/cmp_wrapped_gn4124/p2l_data_i
add wave -noupdate -group {P2L DATA} -color {Blue Violet} /main/DUT/inst_spec_base/gen_with_gennum/cmp_gn4124_core/cmp_wrapped_gn4124/p2l_dframe_i
add wave -noupdate -group {P2L DATA} -color {Blue Violet} /main/DUT/inst_spec_base/gen_with_gennum/cmp_gn4124_core/cmp_wrapped_gn4124/p2l_valid_i
add wave -noupdate -group {L2P DATA} -color {Steel Blue} /main/DUT/inst_spec_base/gen_with_gennum/cmp_gn4124_core/cmp_wrapped_gn4124/l2p_data_o
add wave -noupdate -group {L2P DATA} -color {Steel Blue} /main/DUT/inst_spec_base/gen_with_gennum/cmp_gn4124_core/cmp_wrapped_gn4124/l2p_dframe_o
add wave -noupdate -group {L2P DATA} -color {Steel Blue} /main/DUT/inst_spec_base/gen_with_gennum/cmp_gn4124_core/cmp_wrapped_gn4124/l2p_valid_o
TreeUpdate [SetDefaultTree]
WaveRestoreCursors {{Cursor 1} {11653084600 fs} 0}
quietly wave cursor active 1
configure wave -namecolwidth 350
configure wave -valuecolwidth 100
configure wave -justifyvalue left
configure wave -signalnamewidth 2
configure wave -snapdistance 10
configure wave -datasetprefix 0
configure wave -rowmargin 4
configure wave -childrowmargin 2
configure wave -gridoffset 400000
configure wave -gridperiod 800000
configure wave -griddelta 40
configure wave -timeline 0
configure wave -timelineunits ns
update
WaveRestoreZoom {0 fs} {57075202950 fs}
