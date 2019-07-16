-------------------------------------------------------------------------------
-- Title      : SPEC full
-- Project    : SPEC
-- URL        : http://www.ohwr.org/projects/spec
-------------------------------------------------------------------------------
-- Company    : CERN (BE-CO-HT)
-- Created    : 2017-02-20
-- Last update: 2019-07-16
-- Standard   : VHDL'93
-------------------------------------------------------------------------------
-- Description: SPEC full.
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
use work.wishbone_pkg.all;

entity spec_full is
  generic (
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

    --  DDR3
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
    ddr_we_n_o    : out   std_logic
  );
end entity spec_full;

architecture top of spec_full is
  signal clk_sys_62m5  : std_logic;
  signal rst_sys_62m5_n  : std_logic;

  signal gn_wb_out         : t_wishbone_master_out;
  signal gn_wb_in          : t_wishbone_master_in;
begin
  inst_template: entity work.spec_template_wr
    generic map (
      g_with_vic => True,
      g_with_onewire => False,
      g_with_spi => False,
      g_with_ddr => True,
      g_dpram_initf => g_dpram_initf,
      g_simulation => g_simulation
    )
    port map (
      clk_125m_pllref_p_i => clk_125m_pllref_p_i,
      clk_125m_pllref_n_i => clk_125m_pllref_n_i,
      gn_rst_n_i => gn_rst_n,
      gn_p2l_clk_n_i => gn_p2l_clk_n,
      gn_p2l_clk_p_i => gn_p2l_clk_p,
      gn_p2l_rdy_o => gn_p2l_rdy,
      gn_p2l_dframe_i => gn_p2l_dframe,
      gn_p2l_valid_i => gn_p2l_valid,
      gn_p2l_data_i => gn_p2l_data,
      gn_p_wr_req_i => gn_p_wr_req,
      gn_p_wr_rdy_o => gn_p_wr_rdy,
      gn_rx_error_o => gn_rx_error,
      gn_l2p_clk_n_o => gn_l2p_clk_n,
      gn_l2p_clk_p_o => gn_l2p_clk_p,
      gn_l2p_dframe_o => gn_l2p_dframe,
      gn_l2p_valid_o => gn_l2p_valid,
      gn_l2p_edb_o => gn_l2p_edb,
      gn_l2p_data_o => gn_l2p_data,
      gn_l2p_rdy_i => gn_l2p_rdy,
      gn_l_wr_rdy_i => gn_l_wr_rdy,
      gn_p_rd_d_rdy_i => gn_p_rd_d_rdy,
      gn_tx_error_i => gn_tx_error,
      gn_vc_rdy_i => gn_vc_rdy,
      gn_gpio_b => gn_gpio,
      fmc0_scl_b => fmc0_scl_b,
      fmc0_sda_b => fmc0_sda_b,
      fmc0_prsnt_m2c_n_i => fmc0_prsnt_m2c_n_i,
      onewire_b => onewire_b,
      spi_sclk_o => spi_sclk_o,
      spi_ncs_o => spi_ncs_o,
      spi_mosi_o => spi_mosi_o,
      spi_miso_i => spi_miso_i,
      pcbrev_i => pcbrev_i,
      led_act_o => led_act_o,
      led_link_o => led_link_o,
      button1_i => button1_i,
      uart_rxd_i => uart_rxd_i,
      uart_txd_o => uart_txd_o,
      clk_20m_vcxo_i => clk_20m_vcxo_i,
      clk_125m_gtp_n_i => clk_125m_gtp_n_i,
      clk_125m_gtp_p_i => clk_125m_gtp_p_i,
      plldac_sclk_o => plldac_sclk_o,
      plldac_din_o => plldac_din_o,
      pll25dac_cs_n_o => pll25dac_cs_n_o,
      pll20dac_cs_n_o => pll20dac_cs_n_o,
      sfp_txp_o => sfp_txp_o,
      sfp_txn_o => sfp_txn_o,
      sfp_rxp_i => sfp_rxp_i,
      sfp_rxn_i => sfp_rxn_i,
      sfp_mod_def0_i => sfp_mod_def0_i,
      sfp_mod_def1_b => sfp_mod_def1_b,
      sfp_mod_def2_b => sfp_mod_def2_b,
      sfp_rate_select_o => sfp_rate_select_o,
      sfp_tx_fault_i => sfp_tx_fault_i,
      sfp_tx_disable_o => sfp_tx_disable_o,
      sfp_los_i => sfp_los_i,
      ddr_a_o      => ddr_a_o,
      ddr_ba_o     => ddr_ba_o,
      ddr_cas_n_o  => ddr_cas_n_o,
      ddr_ck_n_o   => ddr_ck_n_o,
      ddr_ck_p_o   => ddr_ck_p_o,
      ddr_cke_o    => ddr_cke_o,
      ddr_dq_b     => ddr_dq_b,
      ddr_ldm_o    => ddr_ldm_o,
      ddr_ldqs_n_b => ddr_ldqs_n_b,
      ddr_ldqs_p_b => ddr_ldqs_p_b,
      ddr_odt_o    => ddr_odt_o,
      ddr_ras_n_o  => ddr_ras_n_o,
      ddr_reset_n_o => ddr_reset_n_o,
      ddr_rzq_b    => ddr_rzq_b,
      ddr_udm_o    => ddr_udm_o,
      ddr_udqs_n_b => ddr_udqs_n_b,
      ddr_udqs_p_b => ddr_udqs_p_b,
      ddr_we_n_o   => ddr_we_n_o,
  
      ddr_dma_clk_i  => clk_sys_62m5,
      ddr_dma_rst_n_i => rst_sys_62m5_n,
      ddr_dma_wb_i.cyc => '0',
      ddr_dma_wb_i.stb => '0',
      ddr_dma_wb_i.adr => x"0000_0000",
      ddr_dma_wb_i.sel => x"00",
      ddr_dma_wb_i.we => '0',
      ddr_dma_wb_i.dat => x"0000_0000_0000_0000",
      ddr_dma_wb_o    => open,

      clk_sys_62m5_o    => clk_sys_62m5,
      rst_sys_62m5_n_o  => rst_sys_62m5_n,

      --  Everything is handled by the carrier.
      app_wb_o         => gn_wb_out,
      app_wb_i         => gn_wb_in
    );
    gn_wb_in <= (ack => '1', err | rty | stall => '0', dat => (others => '0'));
end architecture top;
