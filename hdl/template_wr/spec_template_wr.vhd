-------------------------------------------------------------------------------
-- Title      : WRPC reference design for SPEC
-- Project    : WR PTP Core
-- URL        : http://www.ohwr.org/projects/wr-cores/wiki/Wrpc_core
-------------------------------------------------------------------------------
-- File       : spec_wr_ref_top.vhd
-- Author(s)  : Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
-- Company    : CERN (BE-CO-HT)
-- Created    : 2017-02-20
-- Last update: 2019-04-26
-- Standard   : VHDL'93
-------------------------------------------------------------------------------
-- Description: Top-level file for the WRPC reference design on the SPEC.
--
-- This is a reference top HDL that instanciates the WR PTP Core together with
-- its peripherals to be run on a SPEC card.
--
-- There are two main usecases for this HDL file:
-- * let new users easily synthesize a WR PTP Core bitstream that can be run on
--   reference hardware
-- * provide a reference top HDL file showing how the WRPC can be instantiated
--   in HDL projects.
--
-- SPEC:  http://www.ohwr.org/projects/spec/
--
-------------------------------------------------------------------------------
-- Copyright (c) 2017-2018 CERN
-------------------------------------------------------------------------------
-- GNU LESSER GENERAL PUBLIC LICENSE
--
-- This source file is free software; you can redistribute it
-- and/or modify it under the terms of the GNU Lesser General
-- Public License as published by the Free Software Foundation;
-- either version 2.1 of the License, or (at your option) any
-- later version.
--
-- This source is distributed in the hope that it will be
-- useful, but WITHOUT ANY WARRANTY; without even the implied
-- warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
-- PURPOSE.  See the GNU Lesser General Public License for more
-- details.
--
-- You should have received a copy of the GNU Lesser General
-- Public License along with this source; if not, download it
-- from http://www.gnu.org/licenses/lgpl-2.1.html
--
-------------------------------------------------------------------------------

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

use work.gencores_pkg.all;
use work.wishbone_pkg.all;
use work.ddr3_ctrl_pkg.all;
use work.gn4124_core_pkg.all;
use work.wr_xilinx_pkg.all;
use work.wr_board_pkg.all;
use work.wr_spec_pkg.all;

library unisim;
use unisim.vcomponents.all;

entity spec_template_wr is
  generic (
    --  If true, instantiate a VIC
    g_with_vic : boolean := True;
    g_with_onewire : boolean := True;
    g_with_spi : boolean := True;
    g_with_wr : boolean := True;
    g_CALIB_SOFT_IP : string  := "TRUE";
    g_DPRAM_INITF : string := "../../../../wr-cores/bin/wrpc/wrc_phy8.bram";
    -- Simulation-mode enable parameter. Set by default (synthesis) to 0, and
    -- changed to non-zero in the instantiation of the top level DUT in the testbench.
    -- Its purpose is to reduce some internal counters/timeouts to speed up simulations.
    g_SIMULATION : integer := 0
  );
  port (
    ---------------------------------------------------------------------------
    -- Clocks/resets
    ---------------------------------------------------------------------------

    clk_125m_pllref_p_i : in std_logic;           -- 125 MHz PLL reference
    clk_125m_pllref_n_i : in std_logic;

    ---------------------------------------------------------------------------
    -- GN4124 PCIe bridge signals
    ---------------------------------------------------------------------------
    -- From GN4124 Local bus
    gn_rst_n : in std_logic; -- Reset from GN4124 (RSTOUT18_N)
    -- PCIe to Local [Inbound Data] - RX
    gn_p2l_clk_n  : in  std_logic;       -- Receiver Source Synchronous Clock-
    gn_p2l_clk_p  : in  std_logic;       -- Receiver Source Synchronous Clock+
    gn_p2l_rdy    : out std_logic;       -- Rx Buffer Full Flag
    gn_p2l_dframe : in  std_logic;       -- Receive Frame
    gn_p2l_valid  : in  std_logic;       -- Receive Data Valid
    gn_p2l_data   : in  std_logic_vector(15 downto 0);  -- Parallel receive data
    -- Inbound Buffer Request/Status
    gn_p_wr_req   : in  std_logic_vector(1 downto 0);  -- PCIe Write Request
    gn_p_wr_rdy   : out std_logic_vector(1 downto 0);  -- PCIe Write Ready
    gn_rx_error   : out std_logic;                     -- Receive Error
    -- Local to Parallel [Outbound Data] - TX
    gn_l2p_clk_n  : out std_logic;       -- Transmitter Source Synchronous Clock-
    gn_l2p_clk_p  : out std_logic;       -- Transmitter Source Synchronous Clock+
    gn_l2p_dframe : out std_logic;       -- Transmit Data Frame
    gn_l2p_valid  : out std_logic;       -- Transmit Data Valid
    gn_l2p_edb    : out std_logic;       -- Packet termination and discard
    gn_l2p_data   : out std_logic_vector(15 downto 0);  -- Parallel transmit data
    -- Outbound Buffer Status
    gn_l2p_rdy    : in std_logic;                     -- Tx Buffer Full Flag
    gn_l_wr_rdy   : in std_logic_vector(1 downto 0);  -- Local-to-PCIe Write
    gn_p_rd_d_rdy : in std_logic_vector(1 downto 0);  -- PCIe-to-Local Read Response Data Ready
    gn_tx_error   : in std_logic;                     -- Transmit Error
    gn_vc_rdy     : in std_logic_vector(1 downto 0);  -- Channel ready
    -- General Purpose Interface
    gn_gpio : inout std_logic_vector(1 downto 0);  -- gn_gpio[0] -> GN4124 GPIO8
                                                   -- gn_gpio[1] -> GN4124 GPIO9

    -- I2C interface for accessing FMC EEPROM.
    fmc0_scl_b : inout std_logic;
    fmc0_sda_b : inout std_logic;

    --  FMC presence  (there is a pull-up)
    fmc0_prsnt_m2c_n_i: in std_logic;

    ---------------------------------------------------------------------------
    -- Onewire interface
    ---------------------------------------------------------------------------

    onewire_b : inout std_logic;

    ---------------------------------------------------------------------------
    -- Flash memory SPI interface
    ---------------------------------------------------------------------------

    spi_sclk_o : out std_logic;
    spi_ncs_o  : out std_logic;
    spi_mosi_o : out std_logic;
    spi_miso_i : in  std_logic;

    
    ---------------------------------------------------------------------------
    -- Miscellanous SPEC pins
    ---------------------------------------------------------------------------

    -- PCB version
    pcbrev_i : in std_logic_vector(3 downto 0);

    -- Red LED next to the SFP: blinking indicates that packets are being
    -- transferred.
    led_act_o   : out std_logic;
    -- Green LED next to the SFP: indicates if the link is up.
    led_link_o : out std_logic;

    button1_i   : in  std_logic;

    ---------------------------------------------------------------------------
    -- UART
    ---------------------------------------------------------------------------

    uart_rxd_i : in  std_logic;
    uart_txd_o : out std_logic;

    -- Local oscillators
    clk_20m_vcxo_i : in std_logic;                -- 20MHz VCXO clock

    clk_125m_gtp_n_i : in std_logic;              -- 125 MHz GTP reference
    clk_125m_gtp_p_i : in std_logic;
                                               
    ---------------------------------------------------------------------------
    -- SPI interface to DACs
    ---------------------------------------------------------------------------

    plldac_sclk_o     : out std_logic;
    plldac_din_o      : out std_logic;
    pll25dac_cs_n_o : out std_logic; --cs1
    pll20dac_cs_n_o : out std_logic; --cs2

    ---------------------------------------------------------------------------
    -- SFP I/O for transceiver
    ---------------------------------------------------------------------------

    sfp_txp_o         : out   std_logic;
    sfp_txn_o         : out   std_logic;
    sfp_rxp_i         : in    std_logic;
    sfp_rxn_i         : in    std_logic;
    sfp_mod_def0_i    : in    std_logic;          -- sfp detect
    sfp_mod_def1_b    : inout std_logic;          -- scl
    sfp_mod_def2_b    : inout std_logic;          -- sda
    sfp_rate_select_o : out   std_logic;
    sfp_tx_fault_i    : in    std_logic;
    sfp_tx_disable_o  : out   std_logic;
    sfp_los_i         : in    std_logic;

    ------------------------------------------
    -- DDR (bank 3)
    ------------------------------------------
    ddr_a_o       : out   std_logic_vector(13 downto 0);
    ddr_ba_o      : out   std_logic_vector(2 downto 0);
    ddr_cas_n_o   : out   std_logic;
    ddr_ck_n_o    : out   std_logic;
    ddr_ck_p_o    : out   std_logic;
    ddr_cke_o     : out   std_logic;
    ddr_dq_b      : inout std_logic_vector(15 downto 0);
    ddr_ldm_o     : out   std_logic;
    ddr_ldqs_n_b  : inout std_logic;
    ddr_ldqs_p_b  : inout std_logic;
    ddr_odt_o     : out   std_logic;
    ddr_ras_n_o   : out   std_logic;
    ddr_reset_n_o : out   std_logic;
    ddr_rzq_b     : inout std_logic;
    ddr_udm_o     : out   std_logic;
    ddr_udqs_n_b  : inout std_logic;
    ddr_udqs_p_b  : inout std_logic;
    ddr_we_n_o    : out   std_logic;

    --  Direct access to the DDR-3
    ddr_dma_clk_i   : in  std_logic;
    ddr_dma_rst_n_i : in std_logic;
    ddr_dma_wb_i    : in  t_wishbone_slave_data64_in;
    ddr_dma_wb_o    : out t_wishbone_slave_data64_out;

    --  User part

    --  Clocks and reset.
    clk_sys_62m5_o    : out std_logic;
    rst_sys_62m5_n_o  : out std_logic;

    --  The wishbone bus from the gennum/host.
    --  Addresses 0-0x1fff must be routed to the carrier part.
    gn_wb_out         : out t_wishbone_master_out;
    gn_wb_in          : in  t_wishbone_master_in;

    --  The wishbone bus to the carrier part.
    carrier_wb_out    : out t_wishbone_slave_out;
    carrier_wb_in     : in  t_wishbone_slave_in
  
  );
end entity spec_template_wr;

architecture top of spec_template_wr is
  -- WRPC Xilinx platform auxiliary clock configuration, used for DDR clock
  constant c_WRPC_PLL_CONFIG : t_auxpll_cfg_array := (
    0      => (enabled => TRUE, bufg_en => TRUE, divide => 3),
    others => c_AUXPLL_CFG_DEFAULT);
  
  signal clk_sys          : std_logic;  -- 62.5Mhz

  signal clk_pll_aux        : std_logic_vector(3 downto 0);
  signal rst_pll_aux_n   : std_logic_vector(3 downto 0) := (others => '0');

  --  DDR
  signal clk_ddr_333m       : std_logic;
  signal rst_ddr_333m_n  : std_logic := '0';
  signal ddr_rst         : std_logic := '1';
  signal ddr_status     : std_logic_vector(31 downto 0);
  signal ddr_calib_done : std_logic;

  -- GN4124 core DMA port to DDR wishbone bus
  signal gn_wb_ddr_in  : t_wishbone_master_in;
  signal gn_wb_ddr_out : t_wishbone_master_out;
 
  signal gennum_status : std_logic_vector(31 downto 0);
  
  signal metadata_addr : std_logic_vector(5 downto 2);
  signal metadata_data : std_logic_vector(31 downto 0);

  signal therm_id_in          : t_wishbone_master_in;
  signal therm_id_out         : t_wishbone_master_out;

  -- i2c controllers to the fmcs
  signal fmc_i2c_in           : t_wishbone_master_in;
  signal fmc_i2c_out          : t_wishbone_master_out;

  -- dma registers for the gennum core
  signal dma_in               : t_wishbone_master_in;
  signal dma_out              : t_wishbone_master_out;

  -- spi controller to the flash
  signal flash_spi_in         : t_wishbone_master_in;
  signal flash_spi_out        : t_wishbone_master_out;

  -- vector interrupt controller
  signal vic_in               : t_wishbone_master_in;
  signal vic_out              : t_wishbone_master_out;

  -- white-rabbit core
  signal wrc_in               : t_wishbone_master_in;
  signal wrc_out              : t_wishbone_master_out;
  signal wrc_out_sh           : t_wishbone_master_out;

  signal csr_rst_app : std_logic;
  signal csr_rst_gbl : std_logic;

  signal rst_app_n : std_logic;
  signal rst_gbl_n : std_logic;

  signal fmc0_scl_out, fmc0_sda_out : std_logic;
  signal fmc0_scl_oen, fmc0_sda_oen : std_logic;

  signal fmc_presence : std_logic_vector(31 downto 0);

  signal irq_master : std_logic;
  
  constant num_interrupts : natural := 3;
  signal irqs : std_logic_vector(num_interrupts - 1 downto 0);

  -- clock and reset
  signal rst_sys_62m5_n : std_logic;
  signal rst_ref_125m_n : std_logic;
  signal clk_ref_125m   : std_logic;
  signal clk_ext_10m    : std_logic;

  -- I2C EEPROM
  signal eeprom_sda_in  : std_logic;
  signal eeprom_sda_out : std_logic;
  signal eeprom_scl_in  : std_logic;
  signal eeprom_scl_out : std_logic;

  -- SFP
  signal sfp_sda_in  : std_logic;
  signal sfp_sda_out : std_logic;
  signal sfp_scl_in  : std_logic;
  signal sfp_scl_out : std_logic;

  -- OneWire
  signal onewire_data : std_logic;
  signal onewire_oe   : std_logic;

  -- LEDs and GPIO
  signal wrc_abscal_txts_out : std_logic;
  signal wrc_abscal_rxts_out : std_logic;
  signal wrc_pps_out : std_logic;
  signal wrc_pps_led : std_logic;
  signal wrc_pps_in  : std_logic;
begin  -- architecture top

  ------------------------------------------------------------------------------
  -- GN4124 interface
  ------------------------------------------------------------------------------
  cmp_gn4124_core : gn4124_core
    generic map (
      g_WBM_TO_WB_FIFO_SIZE         => 16,
      g_WBM_TO_WB_FIFO_FULL_THRES   => 12,
      g_WBM_FROM_WB_FIFO_SIZE       => 16,
      g_WBM_FROM_WB_FIFO_FULL_THRES => 12,
      g_P2L_FIFO_SIZE               => 256,
      g_P2L_FIFO_FULL_THRES         => 175,
      g_L2P_ADDR_FIFO_FULL_SIZE     => 256,
      g_L2P_ADDR_FIFO_FULL_THRES    => 175,
      g_L2P_DATA_FIFO_FULL_SIZE     => 256,
      g_L2P_DATA_FIFO_FULL_THRES    => 175)

    port map (
      ---------------------------------------------------------
      -- Control and status
      rst_n_a_i => gn_RST_N,
      status_o  => gennum_status,

      ---------------------------------------------------------
      -- P2L Direction
      --
      -- Source Sync DDR related signals
      p2l_clk_p_i  => gn_P2L_CLK_p,
      p2l_clk_n_i  => gn_P2L_CLK_n,
      p2l_data_i   => gn_P2L_DATA,
      p2l_dframe_i => gn_P2L_DFRAME,
      p2l_valid_i  => gn_P2L_VALID,
      -- P2L Control
      p2l_rdy_o    => gn_P2L_RDY,
      p_wr_req_i   => gn_P_WR_REQ,
      p_wr_rdy_o   => gn_P_WR_RDY,
      rx_error_o   => gn_RX_ERROR,
      vc_rdy_i     => gn_VC_RDY,

      ---------------------------------------------------------
      -- L2P Direction
      --
      -- Source Sync DDR related signals
      l2p_clk_p_o  => gn_L2P_CLK_p,
      l2p_clk_n_o  => gn_L2P_CLK_n,
      l2p_data_o   => gn_L2P_DATA,
      l2p_dframe_o => gn_L2P_DFRAME,
      l2p_valid_o  => gn_L2P_VALID,
      -- L2P Control
      l2p_edb_o    => gn_L2P_EDB,
      l2p_rdy_i    => gn_L2P_RDY,
      l_wr_rdy_i   => gn_L_WR_RDY,
      p_rd_d_rdy_i => gn_P_RD_D_RDY,
      tx_error_i   => gn_TX_ERROR,

      ---------------------------------------------------------
      -- Interrupt interface
      dma_irq_o => irqs(1 downto 0),
      irq_p_i   => irq_master,
      irq_p_o   => gn_GPIO(0),

      ---------------------------------------------------------
      -- DMA registers wishbone interface (slave classic)
      dma_reg_clk_i => clk_sys,
      dma_reg_rst_n_i => rst_gbl_n,
      dma_reg_adr_i => dma_out.adr,
      dma_reg_dat_i => dma_out.dat,
      dma_reg_sel_i => dma_out.sel,
      dma_reg_stb_i => dma_out.stb,
      dma_reg_we_i  => dma_out.we,
      dma_reg_cyc_i => dma_out.cyc,
      dma_reg_ack_o => dma_in.ack,
      dma_reg_dat_o => dma_in.dat,

      ---------------------------------------------------------
      -- CSR wishbone interface (master pipelined)
      csr_clk_i   => clk_sys,
      csr_rst_n_i => rst_gbl_n,
      csr_adr_o   => gn_wb_out.adr,
      csr_dat_o   => gn_wb_out.dat,
      csr_sel_o   => gn_wb_out.sel,
      csr_stb_o   => gn_wb_out.stb,
      csr_we_o    => gn_wb_out.we,
      csr_cyc_o   => gn_wb_out.cyc,
      csr_dat_i   => gn_wb_in.dat,
      csr_ack_i   => gn_wb_in.ack,
      csr_stall_i => gn_wb_in.stall,
      csr_err_i   => gn_wb_in.err,
      csr_rty_i   => gn_wb_in.rty,

      ---------------------------------------------------------
      -- L2P DMA Interface (Pipelined Wishbone master)
      dma_clk_i => clk_sys,
      dma_rst_n_i => rst_gbl_n,
      dma_adr_o => gn_wb_ddr_out.adr,
      dma_dat_o => gn_wb_ddr_out.dat,
      dma_sel_o => gn_wb_ddr_out.sel,
      dma_stb_o => gn_wb_ddr_out.stb,
      dma_we_o  => gn_wb_ddr_out.we,
      dma_cyc_o => gn_wb_ddr_out.cyc,
      dma_dat_i => gn_wb_ddr_in.dat,
      dma_ack_i => gn_wb_ddr_in.ack,
      dma_stall_i => gn_wb_ddr_in.stall,
      dma_err_i => gn_wb_ddr_in.err,
      dma_rty_i => gn_wb_ddr_in.rty);

    dma_in.err <= '0';
    dma_in.rty <= '0';
    dma_in.stall <= '0';

    i_devs: entity work.spec_template_regs
      port map (
        rst_n_i  => gn_RST_N,
        clk_i    => clk_sys,
        wb_cyc_i   => carrier_wb_in.cyc,
        wb_stb_i   => carrier_wb_in.stb,
        wb_adr_i   => carrier_wb_in.adr (10 downto 0),  -- Word address from gennum
        wb_sel_i   => carrier_wb_in.sel,
        wb_we_i    => carrier_wb_in.we,
        wb_dat_i   => carrier_wb_in.dat,
        wb_ack_o   => carrier_wb_out.ack,
        wb_err_o   => carrier_wb_out.err,
        wb_rty_o   => carrier_wb_out.rty,
        wb_stall_o => carrier_wb_out.stall,
        wb_dat_o   => carrier_wb_out.dat,
    
        -- a ROM containing the carrier metadata
        metadata_addr_o => metadata_addr,
        metadata_data_i => metadata_data,
        metadata_data_o => open,
    
        -- offset to the application metadata
        csr_app_offset_i        => x"0000_0000",
        csr_resets_global_o => csr_rst_gbl,
        csr_resets_appl_o   => csr_rst_app,
    
        -- presence lines for the fmcs
        csr_fmc_presence_i  => fmc_presence,
    
        csr_gn4124_status_i => gennum_status,
        csr_ddr_status_calib_done_i    => ddr_calib_done,
        csr_pcb_rev_rev_i   => pcbrev_i,

        -- Thermometer and unique id
        therm_id_i          => therm_id_in,
        therm_id_o          => therm_id_out,
    
        -- i2c controllers to the fmcs
        fmc_i2c_i           => fmc_i2c_in,
        fmc_i2c_o           => fmc_i2c_out,
    
        -- dma registers for the gennum core
        dma_i               => dma_in,
        dma_o               => dma_out,
    
        -- spi controller to the flash
        flash_spi_i         => flash_spi_in,
        flash_spi_o         => flash_spi_out,
    
        -- vector interrupt controller
        vic_i               => vic_in,
        vic_o               => vic_out,
    
        -- white-rabbit core
        wrc_regs_i          => wrc_in,
        wrc_regs_o          => wrc_out
      );
    
    --  Metadata
    process (clk_sys) is
    begin
      if rising_edge(clk_sys) then
        case metadata_addr is
          when x"0" =>
            --  Vendor ID
            metadata_data <= x"000010dc";
          when x"1" =>
            --  Device ID
            metadata_data <= x"53504543";
          when x"2" =>
            -- Version
            metadata_data <= x"01040000";
          when x"3" =>
            -- BOM
            metadata_data <= x"fffe0000";
          when x"4" | x"5" | x"6" | x"7" =>
            -- source id
            metadata_data <= x"00000000";
          when x"8" =>
            -- capability mask
            metadata_data <= x"00000000";
            if g_with_vic then
              metadata_data(0) <= '1';
            end if;
            if g_with_onewire and not g_with_wr then
              metadata_data(1) <= '1';
            end if;
            if g_with_spi then
              metadata_data(2) <= '1';
            end if;
            --  WRC
            metadata_data(3) <= '1';
          when others =>
            metadata_data <= x"00000000";
        end case;
      end if;
    end process;

    fmc_presence (0) <= not fmc0_prsnt_m2c_n_i;
    fmc_presence (31 downto 1) <= (others => '0');
    
    rst_gbl_n <= rst_sys_62m5_n and (not csr_rst_gbl);
    rst_app_n <= rst_gbl_n and (not csr_rst_app);
    ddr_rst <= not rst_ddr_333m_n or csr_rst_gbl;

    rst_sys_62m5_n_o <= rst_app_n;
    clk_sys_62m5_o <= clk_sys;

    i_i2c: entity work.xwb_i2c_master
      generic map (
        g_interface_mode      => CLASSIC,
        g_address_granularity => BYTE,
        g_num_interfaces      => 1)
      port map (
        clk_sys_i => clk_sys,
        rst_n_i   => rst_gbl_n,
    
        slave_i => fmc_i2c_out,
        slave_o => fmc_i2c_in,
        desc_o  => open,
    
        int_o   => irqs(2),
    
        scl_pad_i (0) => fmc0_scl_b,
        scl_pad_o (0) => fmc0_scl_out,
        scl_padoen_o (0) => fmc0_scl_oen,
        sda_pad_i (0) => fmc0_sda_b,
        sda_pad_o (0) => fmc0_sda_out,
        sda_padoen_o (0) => fmc0_sda_oen
        );
    
    fmc0_scl_b <= fmc0_scl_out when fmc0_scl_oen = '0' else 'Z';
    fmc0_sda_b <= fmc0_sda_out when fmc0_sda_oen = '0' else 'Z';

    g_vic: if g_with_vic generate
      i_vic: entity work.xwb_vic
        generic map (
          g_address_granularity => BYTE,
          g_num_interrupts => num_interrupts
        )
        port map (
          clk_sys_i => clk_sys,
          rst_n_i => rst_gbl_n,
          slave_i => vic_out,
          slave_o => vic_in,
          irqs_i => irqs,
          irq_master_o => irq_master
        );
    end generate;

    g_no_vic: if not g_with_vic generate
      vic_in <= (ack => '1', err => '0', rty => '0', stall => '0', dat => x"00000000");
      irq_master <= '0';
    end generate;

  therm_id_in <= (ack => '1', err => '0', rty => '0', stall => '0',
    dat => (others => '0'));
  flash_spi_in <= (ack => '1', err => '0', rty => '0', stall => '0',
    dat => (others => '0'));
  
  -----------------------------------------------------------------------------
  -- The WR PTP core board package (WB Slave + WB Master #2 (Etherbone))
  -----------------------------------------------------------------------------

  wrc_out_sh <= (cyc => wrc_out.cyc, stb => wrc_out.stb,
                 adr => wrc_out.adr or x"00020000",
                 sel => wrc_out.sel, we => wrc_out.we, dat => wrc_out.dat);

  cmp_xwrc_board_spec : xwrc_board_spec
    generic map (
      g_simulation                => g_simulation,
      g_with_external_clock_input => TRUE,
      g_dpram_initf               => g_dpram_initf,
      g_AUX_PLL_CFG               => c_WRPC_PLL_CONFIG,
      g_fabric_iface              => ETHERBONE)
    port map (
      areset_n_i          => button1_i,
      areset_edge_n_i     => gn_rst_n,
      clk_20m_vcxo_i      => clk_20m_vcxo_i,
      clk_125m_pllref_p_i => clk_125m_pllref_p_i,
      clk_125m_pllref_n_i => clk_125m_pllref_n_i,
      clk_125m_gtp_n_i    => clk_125m_gtp_n_i,
      clk_125m_gtp_p_i    => clk_125m_gtp_p_i,
      clk_10m_ext_i       => clk_ext_10m,

      clk_sys_62m5_o      => clk_sys,
      clk_ref_125m_o      => clk_ref_125m,
      clk_pll_aux_o       => clk_pll_aux,
      rst_sys_62m5_n_o    => rst_sys_62m5_n,
      rst_ref_125m_n_o    => rst_ref_125m_n,
      rst_pll_aux_n_o     => rst_pll_aux_n,

      plldac_sclk_o       => plldac_sclk_o,
      plldac_din_o        => plldac_din_o,
      pll25dac_cs_n_o     => pll25dac_cs_n_o,
      pll20dac_cs_n_o     => pll20dac_cs_n_o,

      sfp_txp_o           => sfp_txp_o,
      sfp_txn_o           => sfp_txn_o,
      sfp_rxp_i           => sfp_rxp_i,
      sfp_rxn_i           => sfp_rxn_i,
      sfp_det_i           => sfp_mod_def0_i,
      sfp_sda_i           => sfp_sda_in,
      sfp_sda_o           => sfp_sda_out,
      sfp_scl_i           => sfp_scl_in,
      sfp_scl_o           => sfp_scl_out,
      sfp_rate_select_o   => sfp_rate_select_o,
      sfp_tx_fault_i      => sfp_tx_fault_i,
      sfp_tx_disable_o    => sfp_tx_disable_o,
      sfp_los_i           => sfp_los_i,

      eeprom_sda_i        => eeprom_sda_in,
      eeprom_sda_o        => eeprom_sda_out,
      eeprom_scl_i        => eeprom_scl_in,
      eeprom_scl_o        => eeprom_scl_out,

      onewire_i           => onewire_data,
      onewire_oen_o       => onewire_oe,
      -- Uart
      uart_rxd_i          => uart_rxd_i,
      uart_txd_o          => uart_txd_o,
      -- SPI Flash
      flash_sclk_o        => spi_sclk_o,
      flash_ncs_o         => spi_ncs_o,
      flash_mosi_o        => spi_mosi_o,
      flash_miso_i        => spi_miso_i,

      wb_slave_o          => wrc_in,
      wb_slave_i          => wrc_out_sh,

      wb_eth_master_o     => open,
      wb_eth_master_i     => open,
      
      abscal_txts_o       => wrc_abscal_txts_out,
      abscal_rxts_o       => wrc_abscal_rxts_out,

      pps_ext_i           => wrc_pps_in,
      pps_p_o             => wrc_pps_out,
      pps_led_o           => wrc_pps_led,
      led_link_o          => led_link_o,
      led_act_o           => led_act_o);

  clk_ddr_333m   <= clk_pll_aux(0);
  rst_ddr_333m_n <= rst_pll_aux_n(0);
  clk_ext_10m <= '0';

  -- Tristates for SFP EEPROM
  sfp_mod_def1_b <= '0' when sfp_scl_out = '0' else 'Z';
  sfp_mod_def2_b <= '0' when sfp_sda_out = '0' else 'Z';
  sfp_scl_in     <= sfp_mod_def1_b;
  sfp_sda_in     <= sfp_mod_def2_b;

  -- tri-state onewire access
  onewire_b    <= '0' when (onewire_oe = '1') else 'Z';
  onewire_data <= onewire_b;

  -- EEPROM I2C tri-states
  g_eeprom: if true generate
    fmc0_sda_b <= '0' when (eeprom_sda_out = '0') else 'Z';
    fmc0_scl_b <= '0' when (eeprom_scl_out = '0') else 'Z';
    eeprom_sda_in <= fmc0_sda_b;
    eeprom_scl_in <= fmc0_scl_b;
  end generate;
  
  wrc_pps_in    <= '0';

  --  DDR3 controller
  cmp_ddr_ctrl_bank3 : entity work.ddr3_ctrl
  generic map(
    g_RST_ACT_LOW        => 0, -- active high reset (simpler internal logic)
    g_BANK_PORT_SELECT   => "SPEC_BANK3_64B_32B",
    g_MEMCLK_PERIOD      => 3000,
    g_SIMULATION         => boolean'image(g_SIMULATION /= 0),
    g_CALIB_SOFT_IP      => g_CALIB_SOFT_IP,
    g_P0_MASK_SIZE       => 8,
    g_P0_DATA_PORT_SIZE  => 64,
    g_P0_BYTE_ADDR_WIDTH => 30,
    g_P1_MASK_SIZE       => 4,
    g_P1_DATA_PORT_SIZE  => 32,
    g_P1_BYTE_ADDR_WIDTH => 30)
  port map (
    clk_i   => clk_ddr_333m,
    rst_n_i => ddr_rst,

    status_o => ddr_status,

    ddr3_dq_b     => ddr_dq_b,
    ddr3_a_o      => ddr_a_o,
    ddr3_ba_o     => ddr_ba_o,
    ddr3_ras_n_o  => ddr_ras_n_o,
    ddr3_cas_n_o  => ddr_cas_n_o,
    ddr3_we_n_o   => ddr_we_n_o,
    ddr3_odt_o    => ddr_odt_o,
    ddr3_rst_n_o  => ddr_reset_n_o,
    ddr3_cke_o    => ddr_cke_o,
    ddr3_dm_o     => ddr_ldm_o,
    ddr3_udm_o    => ddr_udm_o,
    ddr3_dqs_p_b  => ddr_ldqs_p_b,
    ddr3_dqs_n_b  => ddr_ldqs_n_b,
    ddr3_udqs_p_b => ddr_udqs_p_b,
    ddr3_udqs_n_b => ddr_udqs_n_b,
    ddr3_clk_p_o  => ddr_ck_p_o,
    ddr3_clk_n_o  => ddr_ck_n_o,
    ddr3_rzq_b    => ddr_rzq_b,

    wb0_rst_n_i => ddr_dma_rst_n_i,
    wb0_clk_i   => ddr_dma_clk_i,
    wb0_sel_i   => ddr_dma_wb_i.sel,
    wb0_cyc_i   => ddr_dma_wb_i.cyc,
    wb0_stb_i   => ddr_dma_wb_i.stb,
    wb0_we_i    => ddr_dma_wb_i.we,
    wb0_addr_i  => ddr_dma_wb_i.adr,
    wb0_data_i  => ddr_dma_wb_i.dat,
    wb0_data_o  => ddr_dma_wb_o.dat,
    wb0_ack_o   => ddr_dma_wb_o.ack,
    wb0_stall_o => ddr_dma_wb_o.stall,

    p0_cmd_empty_o   => open,
    p0_cmd_full_o    => open,
    p0_rd_full_o     => open,
    p0_rd_empty_o    => open,
    p0_rd_count_o    => open,
    p0_rd_overflow_o => open,
    p0_rd_error_o    => open,
    p0_wr_full_o     => open,
    p0_wr_empty_o    => open,
    p0_wr_count_o    => open,
    p0_wr_underrun_o => open,
    p0_wr_error_o    => open,

    wb1_rst_n_i => rst_gbl_n,
    wb1_clk_i   => clk_sys,
    wb1_sel_i   => gn_wb_ddr_out.sel,
    wb1_cyc_i   => gn_wb_ddr_out.cyc,
    wb1_stb_i   => gn_wb_ddr_out.stb,
    wb1_we_i    => gn_wb_ddr_out.we,
    wb1_addr_i  => gn_wb_ddr_out.adr,
    wb1_data_i  => gn_wb_ddr_out.dat,
    wb1_data_o  => gn_wb_ddr_in.dat,
    wb1_ack_o   => gn_wb_ddr_in.ack,
    wb1_stall_o => gn_wb_ddr_in.stall,

    p1_cmd_empty_o   => open,
    p1_cmd_full_o    => open,
    p1_rd_full_o     => open,
    p1_rd_empty_o    => open,
    p1_rd_count_o    => open,
    p1_rd_overflow_o => open,
    p1_rd_error_o    => open,
    p1_wr_full_o     => open,
    p1_wr_empty_o    => open,
    p1_wr_count_o    => open,
    p1_wr_underrun_o => open,
    p1_wr_error_o    => open
    );

    ddr_dma_wb_o.err <= '0';
    ddr_dma_wb_o.rty <= '0';

    ddr_calib_done <= ddr_status(0);
  
    -- unused Wishbone signals
    gn_wb_ddr_in.err <= '0';
    gn_wb_ddr_in.rty <= '0';

end architecture top;
