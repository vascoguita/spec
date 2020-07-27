--------------------------------------------------------------------------------
-- CERN BE-CO-HT
-- SPEC
-- https://ohwr.org/projects/spec
--------------------------------------------------------------------------------
--
-- unit name:   spec_dma_test
--
-- description: A bitstream for testing DMA with a dummy application that pre-
-- loads the first 32 words (128 bytes) of the DDR with a predefined pattern:
--
-- 0x00: 0x01010101
-- 0x04: 0x02020202
-- ...
-- 0x7c: 0x20202020
--
--------------------------------------------------------------------------------
-- Copyright CERN 2020
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
use IEEE.NUMERIC_STD.all;

library work;
use work.wishbone_pkg.all;

entity spec_dma_test is
  generic (
    g_SIMULATION : boolean := FALSE
    );
  port (
    -- Global ports
    clk_125m_pllref_p_i : in std_logic;  -- 125 MHz PLL reference
    clk_125m_pllref_n_i : in std_logic;

    -- From GN4124 Local bus
    gn_rst_n_i      : in    std_logic;  -- Reset from GN4124 (RSTOUT18_N)
    -- PCIe to Local [Inbound Data] - RX
    gn_p2l_clk_n_i  : in    std_logic;  -- Receiver Source Synchronous Clock-
    gn_p2l_clk_p_i  : in    std_logic;  -- Receiver Source Synchronous Clock+
    gn_p2l_rdy_o    : out   std_logic;  -- Rx Buffer Full Flag
    gn_p2l_dframe_i : in    std_logic;  -- Receive Frame
    gn_p2l_valid_i  : in    std_logic;  -- Receive Data Valid
    gn_p2l_data_i   : in    std_logic_vector(15 downto 0);  -- Parallel receive data
    -- Inbound Buffer Request/Status
    gn_p_wr_req_i   : in    std_logic_vector(1 downto 0);  -- PCIe Write Request
    gn_p_wr_rdy_o   : out   std_logic_vector(1 downto 0);  -- PCIe Write Ready
    gn_rx_error_o   : out   std_logic;  -- Receive Error
    -- Local to Parallel [Outbound Data] - TX
    gn_l2p_clk_n_o  : out   std_logic;  -- Transmitter Source Synchronous Clock-
    gn_l2p_clk_p_o  : out   std_logic;  -- Transmitter Source Synchronous Clock+
    gn_l2p_dframe_o : out   std_logic;  -- Transmit Data Frame
    gn_l2p_valid_o  : out   std_logic;  -- Transmit Data Valid
    gn_l2p_edb_o    : out   std_logic;  -- Packet termination and discard
    gn_l2p_data_o   : out   std_logic_vector(15 downto 0);  -- Parallel transmit data
    -- Outbound Buffer Status
    gn_l2p_rdy_i    : in    std_logic;  -- Tx Buffer Full Flag
    gn_l_wr_rdy_i   : in    std_logic_vector(1 downto 0);  -- Local-to-PCIe Write
    gn_p_rd_d_rdy_i : in    std_logic_vector(1 downto 0);  -- PCIe-to-Local Read Response Data Ready
    gn_tx_error_i   : in    std_logic;  -- Transmit Error
    gn_vc_rdy_i     : in    std_logic_vector(1 downto 0);  -- Channel ready
    -- General Purpose Interface
    gn_gpio_b       : inout std_logic_vector(1 downto 0);  -- gn_gpio[0] -> GN4124 GPIO8
                                                           -- gn_gpio[1] -> GN4124 GPIO9
    -- PCB version
    pcbrev_i        : in    std_logic_vector(3 downto 0);

    button1_n_i : in std_logic;

    -- I2C to the FMC
    fmc0_scl_b : inout std_logic;
    fmc0_sda_b : inout std_logic;

    --  FMC presence  (there is a pull-up)
    fmc0_prsnt_m2c_n_i : in std_logic;

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

end spec_dma_test;

architecture arch of spec_dma_test is
  signal clk_sys_62m5   : std_logic;
  signal rst_sys_62m5_n : std_logic;

  signal gn_wb_out : t_wishbone_master_out;
  signal gn_wb_in  : t_wishbone_master_in;

  signal wb_ddr_out : t_wishbone_master_out;
  signal wb_ddr_in  : t_wishbone_master_in;

  type fsm_state_type is (S_IDLE, S_WRITE, S_DONE);
  signal fsm_current_state : fsm_state_type;

begin
  inst_spec_base : entity work.spec_base_wr
    generic map (
      g_WITH_VIC      => TRUE,
      g_WITH_ONEWIRE  => FALSE,
      g_WITH_SPI      => FALSE,
      g_WITH_DDR      => TRUE,
      g_DDR_DATA_SIZE => 32,
      g_WITH_WR       => FALSE,
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

      fmc0_scl_b         => fmc0_scl_b,
      fmc0_sda_b         => fmc0_sda_b,
      fmc0_prsnt_m2c_n_i => fmc0_prsnt_m2c_n_i,

      pcbrev_i    => pcbrev_i,
      button1_n_i => button1_n_i,

      ddr_a_o       => ddr_a_o,
      ddr_ba_o      => ddr_ba_o,
      ddr_cas_n_o   => ddr_cas_n_o,
      ddr_ck_n_o    => ddr_ck_n_o,
      ddr_ck_p_o    => ddr_ck_p_o,
      ddr_cke_o     => ddr_cke_o,
      ddr_dq_b      => ddr_dq_b,
      ddr_ldm_o     => ddr_ldm_o,
      ddr_ldqs_n_b  => ddr_ldqs_n_b,
      ddr_ldqs_p_b  => ddr_ldqs_p_b,
      ddr_odt_o     => ddr_odt_o,
      ddr_ras_n_o   => ddr_ras_n_o,
      ddr_reset_n_o => ddr_reset_n_o,
      ddr_rzq_b     => ddr_rzq_b,
      ddr_udm_o     => ddr_udm_o,
      ddr_udqs_n_b  => ddr_udqs_n_b,
      ddr_udqs_p_b  => ddr_udqs_p_b,
      ddr_we_n_o    => ddr_we_n_o,

      ddr_dma_clk_i      => clk_sys_62m5,
      ddr_dma_rst_n_i    => rst_sys_62m5_n,
      ddr_dma_wb_cyc_i   => wb_ddr_out.cyc,
      ddr_dma_wb_stb_i   => wb_ddr_out.stb,
      ddr_dma_wb_adr_i   => wb_ddr_out.adr,
      ddr_dma_wb_sel_i   => wb_ddr_out.sel,
      ddr_dma_wb_we_i    => wb_ddr_out.we,
      ddr_dma_wb_dat_i   => wb_ddr_out.dat,
      ddr_dma_wb_ack_o   => wb_ddr_in.ack,
      ddr_dma_wb_stall_o => wb_ddr_in.stall,
      ddr_dma_wb_dat_o   => wb_ddr_in.dat,

      clk_62m5_sys_o   => clk_sys_62m5,
      rst_62m5_sys_n_o => rst_sys_62m5_n,

      spi_miso_i => '0',

      app_wb_o => gn_wb_out,
      app_wb_i => gn_wb_in
      );

  gn_wb_in <= (ack => '1', err | rty | stall => '0', dat => (others => '0'));

  p_fsm : process (clk_sys_62m5) is
    variable pattern : unsigned(7 downto 0) := (others => '0');

  begin  -- process p_fsm
    if rising_edge(clk_sys_62m5) then
      if rst_sys_62m5_n = '0' then
        fsm_current_state <= S_IDLE;
        pattern           := (others => '0');
        wb_ddr_out.adr    <= (others => '0');
        wb_ddr_out.dat    <= (others => '0');
        wb_ddr_out.cyc    <= '0';
        wb_ddr_out.stb    <= '0';
      else

        wb_ddr_out.sel <= "1111";
        wb_ddr_out.we  <= '1';

        case fsm_current_state is

          when S_IDLE =>
            wb_ddr_out.cyc    <= '1';
            fsm_current_state <= S_WRITE;

          when S_WRITE =>
            if wb_ddr_in.stall = '0' then
              pattern                    := pattern + 1;
              wb_ddr_out.cyc             <= '1';
              wb_ddr_out.stb             <= '1';
              wb_ddr_out.dat             <= std_logic_vector(pattern & pattern & pattern & pattern);
              wb_ddr_out.adr(7 downto 0) <= std_logic_vector(pattern - 1);
              if pattern = 32 then
                fsm_current_state <= S_DONE;
              end if;
            end if;


          when S_DONE =>
            if wb_ddr_in.stall = '0' then
              wb_ddr_out.cyc <= '0';
              wb_ddr_out.stb <= '0';
            end if;

        end case;
      end if;
    end if;
  end process p_fsm;

end architecture arch;
