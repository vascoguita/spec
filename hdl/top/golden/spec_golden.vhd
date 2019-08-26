--------------------------------------------------------------------------------
-- CERN BE-CO-HT
-- SPEC
-- https://ohwr.org/projects/spec
--------------------------------------------------------------------------------
--
-- unit name:   spec_golden
--
-- description: SPEC golden design, without WR.
--
--------------------------------------------------------------------------------
-- Copyright CERN 2019
--------------------------------------------------------------------------------
-- Copyright and related rights are licensed under the Solderpad Hardware
-- License, Version 2.0 (the "License"); you may not use this file except
-- in compliance with the License. You may obtain a copy of the License at
-- http://solderpad.org/licenses/SHL-2.0.
-- Unless required by applicable law or agreed to in writing, software,
-- hardware and materials distributed under this License is distributed on an
-- "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
-- or implied. See the License for the specific language governing permissions
-- and limitations under the License.
--------------------------------------------------------------------------------

library IEEE;
use IEEE.STD_LOGIC_1164.all;
use work.wishbone_pkg.all;

entity spec_golden is
  port (
    -- Global ports
    clk_125m_pllref_p_i : in std_logic;  -- 125 MHz PLL reference
    clk_125m_pllref_n_i : in std_logic;

    gn_RST_N : in std_logic;           -- Reset from GN4124 (RSTOUT18_N)

    -- General Purpose Interface
    gn_GPIO : inout std_logic_vector(1 downto 0);  -- GPIO[0] -> GN4124 GPIO8
                                                -- GPIO[1] -> GN4124 GPIO9
    -- PCIe to Local [Inbound Data] - RX
    gn_P2L_RDY    : out std_logic;       -- Rx Buffer Full Flag
    gn_P2L_CLK_n  : in  std_logic;       -- Receiver Source Synchronous Clock-
    gn_P2L_CLK_p  : in  std_logic;       -- Receiver Source Synchronous Clock+
    gn_P2L_DATA   : in  std_logic_vector(15 downto 0);  -- Parallel receive data
    gn_P2L_DFRAME : in  std_logic;       -- Receive Frame
    gn_P2L_VALID  : in  std_logic;       -- Receive Data Valid

    -- Inbound Buffer Request/Status
    gn_P_WR_REQ : in  std_logic_vector(1 downto 0);  -- PCIe Write Request
    gn_P_WR_RDY : out std_logic_vector(1 downto 0);  -- PCIe Write Ready
    gn_RX_ERROR : out std_logic;                     -- Receive Error

    -- Local to Parallel [Outbound Data] - TX
    gn_L2P_DATA   : out std_logic_vector(15 downto 0);  -- Parallel transmit data
    gn_L2P_DFRAME : out std_logic;       -- Transmit Data Frame
    gn_L2P_VALID  : out std_logic;       -- Transmit Data Valid
    gn_L2P_CLK_n  : out std_logic;  -- Transmitter Source Synchronous Clock-
    gn_L2P_CLK_p  : out std_logic;  -- Transmitter Source Synchronous Clock+
    gn_L2P_EDB    : out std_logic;       -- Packet termination and discard

    -- Outbound Buffer Status
    gn_L2P_RDY    : in std_logic;        -- Tx Buffer Full Flag
    gn_L_WR_RDY   : in std_logic_vector(1 downto 0);  -- Local-to-PCIe Write
    gn_P_RD_D_RDY : in std_logic_vector(1 downto 0);  -- PCIe-to-Local Read Response Data Ready
    gn_TX_ERROR   : in std_logic;        -- Transmit Error
    gn_VC_RDY     : in std_logic_vector(1 downto 0);  -- Channel ready

    -- PCB version
    pcbrev_i : in std_logic_vector(3 downto 0);

    -- Font panel LEDs
--  LED_RED   : out std_logic;
--  LED_GREEN : out std_logic;

    button1_i   : in  std_logic;

    -- I2C to the FMC
    fmc0_scl_b : inout std_logic;
    fmc0_sda_b : inout std_logic;

    --  FMC presence  (there is a pull-up)
    fmc0_prsnt_m2c_n_i: in std_logic;

    onewire_b : inout std_logic;

    spi_sclk_o : out std_logic;
    spi_ncs_o  : out std_logic;
    spi_mosi_o : out std_logic;
    spi_miso_i : in  std_logic
    );
end spec_golden;

architecture rtl of spec_golden is
  signal clk_sys_62m5  : std_logic;
  signal rst_sys_62m5_n  : std_logic;

  signal gn_wb_out         : t_wishbone_master_out;
  signal gn_wb_in          : t_wishbone_master_in;
begin
  inst_template: entity work.spec_template_wr
    generic map (
      g_WITH_VIC     => True,
      g_WITH_ONEWIRE => True,
      g_WITH_SPI     => True,
      g_WITH_DDR     => False,
      g_WITH_WR      => False,
      g_simulation   => 0
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
end rtl;
