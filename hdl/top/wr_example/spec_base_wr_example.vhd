--------------------------------------------------------------------------------
-- SPDX-FileCopyrightText: 2020 CERN (home.cern)
--
-- SPDX-License-Identifier: CERN-OHL-W-2.0+
--
-- CERN BE-CO-HT
-- SPEC
-- https://ohwr.org/projects/spec
--------------------------------------------------------------------------------
--
-- unit name:   spec_base_wr_example
--
-- description: Example instantiation of SPEC base with White Rabbit.
--
--------------------------------------------------------------------------------

library IEEE;
use IEEE.STD_LOGIC_1164.all;
use IEEE.NUMERIC_STD.all;

entity spec_base_wr_example is

  generic (
    g_DPRAM_INITF : string  := "../../../../wr-cores/bin/wrpc/wrc_phy8.bram";
    -- Simulation-mode enable parameter. Set by default (synthesis) to 0, and
    -- changed to non-zero in the instantiation of the top level DUT in the testbench.
    -- Its purpose is to reduce some internal counters/timeouts to speed up simulations.
    g_SIMULATION  : boolean := FALSE
    );
  port (
    -- Global ports
    clk_125m_pllref_p_i : in std_logic;
    clk_125m_pllref_n_i : in std_logic;
    clk_20m_vcxo_i      : in std_logic;
    clk_125m_gtp_n_i    : in std_logic;
    clk_125m_gtp_p_i    : in std_logic;

    -- GN4124
    gn_rst_n_i      : in    std_logic;
    gn_p2l_clk_n_i  : in    std_logic;
    gn_p2l_clk_p_i  : in    std_logic;
    gn_p2l_rdy_o    : out   std_logic;
    gn_p2l_dframe_i : in    std_logic;
    gn_p2l_valid_i  : in    std_logic;
    gn_p2l_data_i   : in    std_logic_vector(15 downto 0);
    gn_p_wr_req_i   : in    std_logic_vector(1 downto 0);
    gn_p_wr_rdy_o   : out   std_logic_vector(1 downto 0);
    gn_rx_error_o   : out   std_logic;
    gn_l2p_clk_n_o  : out   std_logic;
    gn_l2p_clk_p_o  : out   std_logic;
    gn_l2p_dframe_o : out   std_logic;
    gn_l2p_valid_o  : out   std_logic;
    gn_l2p_edb_o    : out   std_logic;
    gn_l2p_data_o   : out   std_logic_vector(15 downto 0);
    gn_l2p_rdy_i    : in    std_logic;
    gn_l_wr_rdy_i   : in    std_logic_vector(1 downto 0);
    gn_p_rd_d_rdy_i : in    std_logic_vector(1 downto 0);
    gn_tx_error_i   : in    std_logic;
    gn_vc_rdy_i     : in    std_logic_vector(1 downto 0);
    gn_gpio_b       : inout std_logic_vector(1 downto 0);

    -- PCB version and reset button
    pcbrev_i    : in std_logic_vector(3 downto 0);
    button1_n_i : in std_logic;

    -- I2C to the FMC
    fmc0_scl_b : inout std_logic;
    fmc0_sda_b : inout std_logic;

    -- FMC presence (there is a pull-up)
    fmc0_prsnt_m2c_n_i : in std_logic;

    -- DDR3
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

    -- Onewire
    onewire_b : inout std_logic;

    -- SPI
    spi_sclk_o : out std_logic;
    spi_ncs_o  : out std_logic;
    spi_mosi_o : out std_logic;
    spi_miso_i : in  std_logic;

    -- Red LED next to the SFP: blinking indicates that packets are being
    -- transferred.
    led_act_o  : out std_logic;
    -- Green LED next to the SFP: indicates if the link is up.
    led_link_o : out std_logic;

    -- UART
    uart_rxd_i : in  std_logic;
    uart_txd_o : out std_logic;

    -- SPI interface to DACs
    plldac_sclk_o   : out std_logic;
    plldac_din_o    : out std_logic;
    pll25dac_cs_n_o : out std_logic;  --cs1
    pll20dac_cs_n_o : out std_logic;  --cs2

    -- SFP I/O for transceiver
    sfp_txp_o         : out   std_logic;
    sfp_txn_o         : out   std_logic;
    sfp_rxp_i         : in    std_logic;
    sfp_rxn_i         : in    std_logic;
    sfp_mod_def0_i    : in    std_logic;  -- sfp detect
    sfp_mod_def1_b    : inout std_logic;  -- scl
    sfp_mod_def2_b    : inout std_logic;  -- sda
    sfp_rate_select_o : out   std_logic;
    sfp_tx_fault_i    : in    std_logic;
    sfp_tx_disable_o  : out   std_logic;
    sfp_los_i         : in    std_logic
    );

end entity spec_base_wr_example;

architecture arch of spec_base_wr_example is

begin

  inst_spec_base : entity work.spec_base_wr
    generic map (
      g_WITH_VIC      => TRUE,
      g_WITH_ONEWIRE  => FALSE,
      g_WITH_SPI      => FALSE,
      g_WITH_DDR      => TRUE,
      g_DDR_DATA_SIZE => 32,
      g_WITH_WR       => TRUE,
      g_DPRAM_INITF   => g_DPRAM_INITF,
      g_SIMULATION    => g_SIMULATION
      )
    port map (
      clk_125m_pllref_p_i => clk_125m_pllref_p_i,
      clk_125m_pllref_n_i => clk_125m_pllref_n_i,
      gn_rst_n_i          => gn_rst_n_i,
      gn_p2l_clk_n_i      => gn_p2l_clk_n_i,
      gn_p2l_clk_p_i      => gn_p2l_clk_p_i,
      gn_p2l_rdy_o        => gn_p2l_rdy_o,
      gn_p2l_dframe_i     => gn_p2l_dframe_i,
      gn_p2l_valid_i      => gn_p2l_valid_i,
      gn_p2l_data_i       => gn_p2l_data_i,
      gn_p_wr_req_i       => gn_p_wr_req_i,
      gn_p_wr_rdy_o       => gn_p_wr_rdy_o,
      gn_rx_error_o       => gn_rx_error_o,
      gn_l2p_clk_n_o      => gn_l2p_clk_n_o,
      gn_l2p_clk_p_o      => gn_l2p_clk_p_o,
      gn_l2p_dframe_o     => gn_l2p_dframe_o,
      gn_l2p_valid_o      => gn_l2p_valid_o,
      gn_l2p_edb_o        => gn_l2p_edb_o,
      gn_l2p_data_o       => gn_l2p_data_o,
      gn_l2p_rdy_i        => gn_l2p_rdy_i,
      gn_l_wr_rdy_i       => gn_l_wr_rdy_i,
      gn_p_rd_d_rdy_i     => gn_p_rd_d_rdy_i,
      gn_tx_error_i       => gn_tx_error_i,
      gn_vc_rdy_i         => gn_vc_rdy_i,
      gn_gpio_b           => gn_gpio_b,
      fmc0_scl_b          => fmc0_scl_b,
      fmc0_sda_b          => fmc0_sda_b,
      fmc0_prsnt_m2c_n_i  => fmc0_prsnt_m2c_n_i,
      onewire_b           => onewire_b,
      spi_sclk_o          => spi_sclk_o,
      spi_ncs_o           => spi_ncs_o,
      spi_mosi_o          => spi_mosi_o,
      spi_miso_i          => spi_miso_i,
      pcbrev_i            => pcbrev_i,
      led_act_o           => led_act_o,
      led_link_o          => led_link_o,
      button1_n_i         => button1_n_i,
      ddr_a_o             => ddr_a_o,
      ddr_ba_o            => ddr_ba_o,
      ddr_cas_n_o         => ddr_cas_n_o,
      ddr_ck_n_o          => ddr_ck_n_o,
      ddr_ck_p_o          => ddr_ck_p_o,
      ddr_cke_o           => ddr_cke_o,
      ddr_dq_b            => ddr_dq_b,
      ddr_ldm_o           => ddr_ldm_o,
      ddr_ldqs_n_b        => ddr_ldqs_n_b,
      ddr_ldqs_p_b        => ddr_ldqs_p_b,
      ddr_odt_o           => ddr_odt_o,
      ddr_ras_n_o         => ddr_ras_n_o,
      ddr_reset_n_o       => ddr_reset_n_o,
      ddr_rzq_b           => ddr_rzq_b,
      ddr_udm_o           => ddr_udm_o,
      ddr_udqs_n_b        => ddr_udqs_n_b,
      ddr_udqs_p_b        => ddr_udqs_p_b,
      ddr_we_n_o          => ddr_we_n_o,
      uart_rxd_i          => uart_rxd_i,
      uart_txd_o          => uart_txd_o,
      clk_20m_vcxo_i      => clk_20m_vcxo_i,
      clk_125m_gtp_n_i    => clk_125m_gtp_n_i,
      clk_125m_gtp_p_i    => clk_125m_gtp_p_i,
      plldac_sclk_o       => plldac_sclk_o,
      plldac_din_o        => plldac_din_o,
      pll25dac_cs_n_o     => pll25dac_cs_n_o,
      pll20dac_cs_n_o     => pll20dac_cs_n_o,
      sfp_txp_o           => sfp_txp_o,
      sfp_txn_o           => sfp_txn_o,
      sfp_rxp_i           => sfp_rxp_i,
      sfp_rxn_i           => sfp_rxn_i,
      sfp_mod_def0_i      => sfp_mod_def0_i,
      sfp_mod_def1_b      => sfp_mod_def1_b,
      sfp_mod_def2_b      => sfp_mod_def2_b,
      sfp_rate_select_o   => sfp_rate_select_o,
      sfp_tx_fault_i      => sfp_tx_fault_i,
      sfp_tx_disable_o    => sfp_tx_disable_o,
      sfp_los_i           => sfp_los_i
      );

end architecture arch;
