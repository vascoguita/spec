`timescale 1ns/1ps

`include "gn4124_bfm.svh"

`define DMA_BASE 'h00c0
`define VIC_BASE 'h0100

module main;

   reg rst_n = 0;

   reg clk_125m_pllref = 0;
   reg clk_20m_vcxo = 0;

   initial begin
      repeat(20) @(posedge clk_125m_pllref);
      rst_n = 1;
   end

   IGN4124PCIMaster i_gn4124 ();

   wire ddr_cas_n, ddr_ck_p, ddr_ck_n, ddr_cke;
   wire [1:0] ddr_dm, ddr_dqs_p, ddr_dqs_n;
   wire ddr_odt, ddr_ras_n, ddr_reset_n, ddr_we_n;
   wire [15:0] ddr_dq;
   wire [13:0] ddr_a;
   wire [2:0]  ddr_ba;
   wire        ddr_rzq;

   pulldown(ddr_rzq);

   // 125Mhz
   always #4ns clk_125m_pllref <= ~clk_125m_pllref;

   spec_dma_test
     #(
       .g_dma_use_pci_clk (0),
       .g_SIMULATION(1)
       )
   DUT
     (
      .button1_n_i              (rst_n),
      .clk_125m_pllref_p_i      (clk_125m_pllref),
      .clk_125m_pllref_n_i      (~clk_125m_pllref),
      .gn_rst_n_i                (i_gn4124.rst_n),
      .gn_p2l_clk_n_i            (i_gn4124.p2l_clk_n),
      .gn_p2l_clk_p_i            (i_gn4124.p2l_clk_p),
      .gn_p2l_rdy_o              (i_gn4124.p2l_rdy),
      .gn_p2l_dframe_i           (i_gn4124.p2l_dframe),
      .gn_p2l_valid_i            (i_gn4124.p2l_valid),
      .gn_p2l_data_i             (i_gn4124.p2l_data),
      .gn_p_wr_req_i             (i_gn4124.p_wr_req),
      .gn_p_wr_rdy_o             (i_gn4124.p_wr_rdy),
      .gn_rx_error_o             (i_gn4124.rx_error),
      .gn_l2p_clk_n_o            (i_gn4124.l2p_clk_n),
      .gn_l2p_clk_p_o            (i_gn4124.l2p_clk_p),
      .gn_l2p_dframe_o           (i_gn4124.l2p_dframe),
      .gn_l2p_valid_o            (i_gn4124.l2p_valid),
      .gn_l2p_edb_o              (i_gn4124.l2p_edb),
      .gn_l2p_data_o             (i_gn4124.l2p_data),
      .gn_l2p_rdy_i              (i_gn4124.l2p_rdy),
      .gn_l_wr_rdy_i             (i_gn4124.l_wr_rdy),
      .gn_p_rd_d_rdy_i           (i_gn4124.p_rd_d_rdy),
      .gn_tx_error_i             (i_gn4124.tx_error),
      .gn_vc_rdy_i               (i_gn4124.vc_rdy),
      .gn_gpio_b                 (),
      .ddr_a_o                   (ddr_a),
      .ddr_ba_o                  (ddr_ba),
      .ddr_cas_n_o               (ddr_cas_n),
      .ddr_ck_n_o                (ddr_ck_n),
      .ddr_ck_p_o                (ddr_ck_p),
      .ddr_cke_o                 (ddr_cke),
      .ddr_dq_b                  (ddr_dq),
      .ddr_ldm_o                 (ddr_dm[0]),
      .ddr_ldqs_n_b              (ddr_dqs_n[0]),
      .ddr_ldqs_p_b              (ddr_dqs_p[0]),
      .ddr_odt_o                 (ddr_odt),
      .ddr_ras_n_o               (ddr_ras_n),
      .ddr_reset_n_o             (ddr_reset_n),
      .ddr_rzq_b                 (ddr_rzq),
      .ddr_udm_o                 (ddr_dm[1]),
      .ddr_udqs_n_b              (ddr_dqs_n[1]),
      .ddr_udqs_p_b              (ddr_dqs_p[1]),
      .ddr_we_n_o                (ddr_we_n)
      );

   ddr3 #
     (
      .DEBUG(0),
      .check_strict_timing(0),
      .check_strict_mrbits(0)
      )
   cmp_ddr0
     (
      .rst_n   (ddr_reset_n),
      .ck      (ddr_ck_p),
      .ck_n    (ddr_ck_n),
      .cke     (ddr_cke),
      .cs_n    (1'b0),
      .ras_n   (ddr_ras_n),
      .cas_n   (ddr_cas_n),
      .we_n    (ddr_we_n),
      .dm_tdqs (ddr_dm),
      .ba      (ddr_ba),
      .addr    (ddr_a),
      .dq      (ddr_dq),
      .dqs     (ddr_dqs_p),
      .dqs_n   (ddr_dqs_n),
      .tdqs_n  (),
      .odt     (ddr_odt)
      );

   typedef enum bit {RD,WR} dma_dir_t;

   task dma_xfer(input CBusAccessor acc,
                 input uint64_t host_addr,
                 input uint32_t start_addr,
                 input uint32_t length,
                 input dma_dir_t dma_dir,
                 input int timeout = 1ms);

      real timeout_time;

      // Configure the VIC
      acc.write(`VIC_BASE + 'h8, 'h7f);
      acc.write(`VIC_BASE + 'h0, 'h1);

      // Setup DMA addresses
      acc.write(`DMA_BASE + 'h08, start_addr);             // dma start addr
      acc.write(`DMA_BASE + 'h0C, host_addr & 'hffffffff); // host addr low
      acc.write(`DMA_BASE + 'h10, host_addr >> 32);        // host addr high
      acc.write(`DMA_BASE + 'h14, length);                 // length in bytes
      acc.write(`DMA_BASE + 'h18, 'h00000000);             // next low
      acc.write(`DMA_BASE + 'h1C, 'h00000000);             // next high

      // Setup DMA direction
      if (dma_dir == RD)
        begin
           acc.write(`DMA_BASE + 'h20, 'h00000000);        // attrib: pcie -> host
           $display("<%t> START DMA READ from 0x%x, %0d bytes",
                    $realtime, start_addr, length);
        end
      else
        begin
           acc.write(`DMA_BASE + 'h20, 'h00000001);        // attrib: host -> pcie
           $display("<%t> START DMA WRITE to 0x%x, %0d bytes",
                    $realtime, start_addr, length);
        end

      // Start transfer
      acc.write(`DMA_BASE + 'h00, 'h00000001);

      // Check for completion/timeout
      timeout_time = $realtime + timeout;
      while (timeout_time > $realtime)
        begin
           if (DUT.inst_spec_base.irqs[2] == 1)
             begin
                $display("<%t> END DMA", $realtime);
                acc.write(`DMA_BASE + 'h04, 'h04);
                acc.write(`VIC_BASE + 'h1c, 'h0);
                return;
             end
           #1us;
        end
      $fatal(1, "<%t> DMA TIMEOUT", $realtime);
   endtask // dma_xfer

   typedef virtual IGN4124PCIMaster vIGN4124PCIMaster;

   task dma_read_pattern(vIGN4124PCIMaster i_gn4124);

      int i;

      uint64_t val, expected;

      CBusAccessor acc;
      acc = i_gn4124.get_accessor();
      acc.set_default_xfer_size(4);

      // Read pattern from device memory
      dma_xfer(acc, 'h20000000, 'h0, 4 * 'h20, RD);
      // Verify pattern
      for (i = 'h00; i < 'h20; i++)
        begin
           expected  = i+1;
           expected |= (i+1) << 8;
           expected |= (i+1) << 16;
           expected |= (i+1) << 24;
           i_gn4124.host_mem_read(i*4, val);
           if (val != expected)
             $fatal(1, "<%t> READ-BACK ERROR at host address 0x%x: expected 0x%8x, got 0x%8x",
                    $realtime, i*4, expected, val);
        end
   endtask // dma_read_pattern

   task dma_test(vIGN4124PCIMaster i_gn4124,
                 input uint32_t word_count);

      int i;

      uint32_t word_addr, word_remain, word_ptr;
      uint64_t val, expected, host_addr;
      uint64_t data_queue[$];

      CBusAccessor acc;
      acc = i_gn4124.get_accessor();
      acc.set_default_xfer_size(4);

      word_addr = $urandom_range(65535 - word_count);

      // Prepare host memory
      for (i = 0; i < word_count; i++)
        begin
           val = $urandom();
           i_gn4124.host_mem_write(i*4, val);
           data_queue.push_back(val);
        end
      // Write data to device memory
      word_ptr = word_addr;
      word_remain = word_count;
      host_addr = 'h20000000;
      while (word_remain != 0)
        begin
           if (word_remain > 1024)
             begin
                dma_xfer(acc, host_addr, word_ptr * 4, 4096, WR);
                word_remain -= 1024;
                word_ptr    += 1024;
                host_addr   += 4096;
             end
           else
             begin
                dma_xfer(acc, host_addr, word_ptr * 4, word_remain * 4, WR);
                word_ptr += word_remain;
                word_remain = 0;
                host_addr = 'h20000000;
             end
        end
      // Clear host memory
      for (i = 0; i < word_count; i++)
        begin
           i_gn4124.host_mem_write(i*4, 0);
        end
      // Read data from device memory
      dma_xfer(acc, host_addr, word_addr * 4, word_count * 4, RD);
      // Compare against written data
      for (i = 0; i < word_count; i++)
        begin
           i_gn4124.host_mem_read(i*4, val);
           expected = data_queue.pop_front();
           if (val != expected)
             $fatal(1, "<%t> READ-BACK ERROR at host address 0x%x: expected 0x%8x, got 0x%8x",
                    $realtime, i*4, expected, val);
        end
   endtask // dma_test

   initial begin

      int i;

      uint64_t val, expected;

      vIGN4124PCIMaster vi_gn4124;

      vi_gn4124= i_gn4124;

      $timeformat (-6, 3, "us", 10);

      $display();
      $display ("Simulation START");
      $display();

      #10us;

      dma_read_pattern(vi_gn4124);

      for (i = 2; i < 13; i++)
        begin
           #1us;
           dma_test(vi_gn4124, 2**i);
        end

      $display();
      $display ("Simulation PASSED");
      $display();

      $finish;

   end

endmodule // main
