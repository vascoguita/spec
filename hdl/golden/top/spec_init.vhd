-------------------------------------------------------------------------------
-- Title      : SPEC Golden Binary
-- Project    : WhiteRabbit
-------------------------------------------------------------------------------
-- File       : spec_init.vhd
-- Author     : Grzegorz Daniluk
-- Company    : CERN BE-CO-HT
-- Created    : 2012-07-17
-- Last update: 2012-11-20
-- Platform   : FPGA-generics
-- Standard   : VHDL
-------------------------------------------------------------------------------
-- Copyright (c) 2012 Grzegorz Daniluk
-------------------------------------------------------------------------------
-- Revisions  :
-- Date        Version  Author          Description
-- 2012-07-17  1.0      greg.d          Created
-------------------------------------------------------------------------------

library IEEE;
use IEEE.STD_LOGIC_1164.all;
use IEEE.NUMERIC_STD.all;

use work.gn4124_core_pkg.all;
use work.gencores_pkg.all;
use work.wrcore_pkg.all;
use work.wr_xilinx_pkg.all;

library UNISIM;
use UNISIM.vcomponents.all;

library work;
use work.wishbone_pkg.all;


entity spec_init is
  generic
    (
      TAR_ADDR_WDTH : integer := 13     -- not used for this project
      );
  port
    (
      -- Global ports
      clk_125m_pllref_p_i : in std_logic;  -- 125 MHz PLL reference
      clk_125m_pllref_n_i : in std_logic;

      L_RST_N : in std_logic;           -- Reset from GN4124 (RSTOUT18_N)

      -- General Purpose Interface
      GPIO : inout std_logic_vector(1 downto 0);  -- GPIO[0] -> GN4124 GPIO8
                                                  -- GPIO[1] -> GN4124 GPIO9
      -- PCIe to Local [Inbound Data] - RX
      P2L_RDY    : out std_logic;       -- Rx Buffer Full Flag
      P2L_CLKn   : in  std_logic;       -- Receiver Source Synchronous Clock-
      P2L_CLKp   : in  std_logic;       -- Receiver Source Synchronous Clock+
      P2L_DATA   : in  std_logic_vector(15 downto 0);  -- Parallel receive data
      P2L_DFRAME : in  std_logic;       -- Receive Frame
      P2L_VALID  : in  std_logic;       -- Receive Data Valid

      -- Inbound Buffer Request/Status
      P_WR_REQ : in  std_logic_vector(1 downto 0);  -- PCIe Write Request
      P_WR_RDY : out std_logic_vector(1 downto 0);  -- PCIe Write Ready
      RX_ERROR : out std_logic;                     -- Receive Error

      -- Local to Parallel [Outbound Data] - TX
      L2P_DATA   : out std_logic_vector(15 downto 0);  -- Parallel transmit data
      L2P_DFRAME : out std_logic;       -- Transmit Data Frame
      L2P_VALID  : out std_logic;       -- Transmit Data Valid
      L2P_CLKn   : out std_logic;  -- Transmitter Source Synchronous Clock-
      L2P_CLKp   : out std_logic;  -- Transmitter Source Synchronous Clock+
      L2P_EDB    : out std_logic;       -- Packet termination and discard

      -- Outbound Buffer Status
      L2P_RDY    : in std_logic;        -- Tx Buffer Full Flag
      L_WR_RDY   : in std_logic_vector(1 downto 0);  -- Local-to-PCIe Write
      P_RD_D_RDY : in std_logic_vector(1 downto 0);  -- PCIe-to-Local Read Response Data Ready
      TX_ERROR   : in std_logic;        -- Transmit Error
      VC_RDY     : in std_logic_vector(1 downto 0);  -- Channel ready

      -- Font panel LEDs
      LED_RED   : out std_logic;
      LED_GREEN : out std_logic;

      fpga_scl_b : inout std_logic;
      fpga_sda_b : inout std_logic;

      button1_i : in std_logic;
      button2_i : in std_logic;

      spi_sclk_o : out std_logic; 
      spi_ncs_o  : out std_logic;
      spi_mosi_o : out std_logic;
      spi_miso_i : in  std_logic;

      -------------------------------------------------------------------------
      -- SFP pins
      -------------------------------------------------------------------------

      sfp_mod_def0_b    : in    std_logic;  -- sfp detect
      sfp_mod_def1_b    : inout std_logic;  -- scl
      sfp_mod_def2_b    : inout std_logic;  -- sda
      sfp_rate_select_b : inout std_logic;
      sfp_tx_fault_i    : in    std_logic;
      sfp_tx_disable_o  : out   std_logic;
      sfp_los_i         : in    std_logic

      );

end spec_init;

architecture rtl of spec_init is

  ------------------------------------------------------------------------------
  -- Components declaration
  ------------------------------------------------------------------------------

  component gn4124_core is
    port(
      ---------------------------------------------------------
      -- Control and status
      rst_n_a_i : in  std_logic;        -- Asynchronous reset from GN4124
      status_o  : out std_logic_vector(31 downto 0);  -- Core status output

      ---------------------------------------------------------
      -- P2L Direction
      --
      -- Source Sync DDR related signals
      p2l_clk_p_i  : in  std_logic;     -- Receiver Source Synchronous Clock+
      p2l_clk_n_i  : in  std_logic;     -- Receiver Source Synchronous Clock-
      p2l_data_i   : in  std_logic_vector(15 downto 0);  -- Parallel receive data
      p2l_dframe_i : in  std_logic;     -- Receive Frame
      p2l_valid_i  : in  std_logic;     -- Receive Data Valid
      -- P2L Control
      p2l_rdy_o    : out std_logic;     -- Rx Buffer Full Flag
      p_wr_req_i   : in  std_logic_vector(1 downto 0);  -- PCIe Write Request
      p_wr_rdy_o   : out std_logic_vector(1 downto 0);  -- PCIe Write Ready
      rx_error_o   : out std_logic;     -- Receive Error
      vc_rdy_i     : in  std_logic_vector(1 downto 0);  -- Virtual channel ready

      ---------------------------------------------------------
      -- L2P Direction
      --
      -- Source Sync DDR related signals
      l2p_clk_p_o  : out std_logic;  -- Transmitter Source Synchronous Clock+
      l2p_clk_n_o  : out std_logic;  -- Transmitter Source Synchronous Clock-
      l2p_data_o   : out std_logic_vector(15 downto 0);  -- Parallel transmit data
      l2p_dframe_o : out std_logic;     -- Transmit Data Frame
      l2p_valid_o  : out std_logic;     -- Transmit Data Valid
      -- L2P Control
      l2p_edb_o    : out std_logic;     -- Packet termination and discard
      l2p_rdy_i    : in  std_logic;     -- Tx Buffer Full Flag
      l_wr_rdy_i   : in  std_logic_vector(1 downto 0);  -- Local-to-PCIe Write
      p_rd_d_rdy_i : in  std_logic_vector(1 downto 0);  -- PCIe-to-Local Read Response Data Ready
      tx_error_i   : in  std_logic;     -- Transmit Error

      ---------------------------------------------------------
      -- Interrupt interface
      dma_irq_o : out std_logic_vector(1 downto 0);  -- Interrupts sources to IRQ manager
      irq_p_i   : in  std_logic;  -- Interrupt request pulse from IRQ manager
      irq_p_o   : out std_logic;  -- Interrupt request pulse to GN4124 GPIO

      ---------------------------------------------------------
      -- DMA registers wishbone interface (slave classic)
      dma_reg_clk_i   : in  std_logic;
      dma_reg_adr_i   : in  std_logic_vector(31 downto 0) := x"00000000";
      dma_reg_dat_i   : in  std_logic_vector(31 downto 0) := x"00000000";
      dma_reg_sel_i   : in  std_logic_vector(3 downto 0)  := x"0";
      dma_reg_stb_i   : in  std_logic                     := '0';
      dma_reg_we_i    : in  std_logic                     := '0';
      dma_reg_cyc_i   : in  std_logic                     := '0';
      dma_reg_dat_o   : out std_logic_vector(31 downto 0);
      dma_reg_ack_o   : out std_logic;
      dma_reg_stall_o : out std_logic;

      ---------------------------------------------------------
      -- CSR wishbone interface (master pipelined)
      csr_clk_i   : in  std_logic;
      csr_adr_o   : out std_logic_vector(31 downto 0);
      csr_dat_o   : out std_logic_vector(31 downto 0);
      csr_sel_o   : out std_logic_vector(3 downto 0);
      csr_stb_o   : out std_logic;
      csr_we_o    : out std_logic;
      csr_cyc_o   : out std_logic;
      csr_dat_i   : in  std_logic_vector(31 downto 0);
      csr_ack_i   : in  std_logic;
      csr_stall_i : in  std_logic;

      ---------------------------------------------------------
      -- DMA wishbone interface (master pipelined)
      dma_clk_i   : in  std_logic;
      dma_adr_o   : out std_logic_vector(31 downto 0);
      dma_dat_o   : out std_logic_vector(31 downto 0);
      dma_sel_o   : out std_logic_vector(3 downto 0);
      dma_stb_o   : out std_logic;
      dma_we_o    : out std_logic;
      dma_cyc_o   : out std_logic;
      dma_dat_i   : in  std_logic_vector(31 downto 0) := x"00000000";
      dma_ack_i   : in  std_logic                     := '0';
      dma_stall_i : in  std_logic                     := '0'
      );
  end component;  --  gn4124_core

  ------------------------------------------------------------------------------
  -- Signals declaration
  ------------------------------------------------------------------------------

  signal pllout_clk_sys       : std_logic;
  signal pllout_clk_fb_pllref : std_logic;

  signal clk_125m_pllref  : std_logic;
  signal clk_sys          : std_logic;

  signal wrc_scl_o : std_logic;
  signal wrc_scl_i : std_logic;
  signal wrc_sda_o : std_logic;
  signal wrc_sda_i : std_logic;
  signal sfp_scl_o : std_logic;
  signal sfp_scl_i : std_logic;
  signal sfp_sda_o : std_logic;
  signal sfp_sda_i : std_logic;

  signal genum_wb_out : t_wishbone_master_out;
  signal genum_wb_in  : t_wishbone_master_in;
  
  signal wb_adr : std_logic_vector(31 downto 0);  --c_BAR0_APERTURE-priv_log2_ceil(c_CSR_WB_SLAVES_NB+1)-1 downto 0);

  constant c_sdb_address : t_wishbone_address := x"00000100";
  constant c_layout : t_sdb_record := f_sdb_embed_device(c_wrc_periph0_sdb, x"00000000");
  ------------------------------------

  signal periph_slv_in : t_wishbone_slave_in_array(0 to 2);
  signal periph_slv_out: t_wishbone_slave_out_array(0 to 2);

begin

  cmp_sys_clk_pll : PLL_BASE
    generic map (
      BANDWIDTH          => "OPTIMIZED",
      CLK_FEEDBACK       => "CLKFBOUT",
      COMPENSATION       => "INTERNAL",
      DIVCLK_DIVIDE      => 1,
      CLKFBOUT_MULT      => 8,
      CLKFBOUT_PHASE     => 0.000,
      CLKOUT0_DIVIDE     => 16,         -- 62.5 MHz
      CLKOUT0_PHASE      => 0.000,
      CLKOUT0_DUTY_CYCLE => 0.500,
      CLKOUT1_DIVIDE     => 16,         -- 125 MHz
      CLKOUT1_PHASE      => 0.000,
      CLKOUT1_DUTY_CYCLE => 0.500,
      CLKOUT2_DIVIDE     => 16,
      CLKOUT2_PHASE      => 0.000,
      CLKOUT2_DUTY_CYCLE => 0.500,
      CLKIN_PERIOD       => 8.0,
      REF_JITTER         => 0.016)
    port map (
      CLKFBOUT => pllout_clk_fb_pllref,
      CLKOUT0  => pllout_clk_sys,
      CLKOUT1  => open,
      CLKOUT2  => open,
      CLKOUT3  => open,
      CLKOUT4  => open,
      CLKOUT5  => open,
      LOCKED   => open,
      RST      => '0',
      CLKFBIN  => pllout_clk_fb_pllref,
      CLKIN    => clk_125m_pllref);

  cmp_clk_sys_buf : BUFG
    port map (
      O => clk_sys,
      I => pllout_clk_sys);

  ------------------------------------------------------------------------------
  -- Local clock from gennum LCLK
  ------------------------------------------------------------------------------
  cmp_pllrefclk_buf : IBUFGDS
    generic map (
      DIFF_TERM    => true,             -- Differential Termination
      IBUF_LOW_PWR => true,  -- Low power (TRUE) vs. performance (FALSE) setting for referenced I/O standards
      IOSTANDARD   => "DEFAULT")
    port map (
      O  => clk_125m_pllref,            -- Buffer output
      I  => clk_125m_pllref_p_i,  -- Diff_p buffer input (connect directly to top-level port)
      IB => clk_125m_pllref_n_i  -- Diff_n buffer input (connect directly to top-level port)
      );

  ------------------------------------------------------------------------------
  -- GN4124 interface
  ------------------------------------------------------------------------------
  cmp_gn4124_core : gn4124_core
    port map
    (
      ---------------------------------------------------------
      -- Control and status
      rst_n_a_i => L_RST_N,
      status_o  => open,

      ---------------------------------------------------------
      -- P2L Direction
      --
      -- Source Sync DDR related signals
      p2l_clk_p_i  => P2L_CLKp,
      p2l_clk_n_i  => P2L_CLKn,
      p2l_data_i   => P2L_DATA,
      p2l_dframe_i => P2L_DFRAME,
      p2l_valid_i  => P2L_VALID,
      -- P2L Control
      p2l_rdy_o    => P2L_RDY,
      p_wr_req_i   => P_WR_REQ,
      p_wr_rdy_o   => P_WR_RDY,
      rx_error_o   => RX_ERROR,
      vc_rdy_i     => VC_RDY,

      ---------------------------------------------------------
      -- L2P Direction
      --
      -- Source Sync DDR related signals
      l2p_clk_p_o  => L2P_CLKp,
      l2p_clk_n_o  => L2P_CLKn,
      l2p_data_o   => L2P_DATA,
      l2p_dframe_o => L2P_DFRAME,
      l2p_valid_o  => L2P_VALID,
      -- L2P Control
      l2p_edb_o    => L2P_EDB,
      l2p_rdy_i    => L2P_RDY,
      l_wr_rdy_i   => L_WR_RDY,
      p_rd_d_rdy_i => P_RD_D_RDY,
      tx_error_i   => TX_ERROR,

      ---------------------------------------------------------
      -- Interrupt interface
      dma_irq_o => open,
      irq_p_i   => '0',
      irq_p_o   => GPIO(0),

      ---------------------------------------------------------
      -- DMA registers wishbone interface (slave classic)
      dma_reg_clk_i => clk_sys,

      ---------------------------------------------------------
      -- CSR wishbone interface (master pipelined)
      csr_clk_i   => clk_sys,
      csr_adr_o   => wb_adr,
      csr_dat_o   => genum_wb_out.dat,
      csr_sel_o   => genum_wb_out.sel,
      csr_stb_o   => genum_wb_out.stb,
      csr_we_o    => genum_wb_out.we,
      csr_cyc_o   => genum_wb_out.cyc,
      csr_dat_i   => genum_wb_in.dat,
      csr_ack_i   => genum_wb_in.ack or genum_wb_in.err,
      csr_stall_i => genum_wb_in.stall,

      ---------------------------------------------------------
      -- L2P DMA Interface (Pipelined Wishbone master)
      dma_clk_i => clk_sys
    );

  genum_wb_out.adr( 1 downto  0) <= (others => '0');
  genum_wb_out.adr(18 downto  2) <= wb_adr(16 downto 0);
  genum_wb_out.adr(31 downto 19) <= (others => '0');

  fpga_scl_b <= '0' when wrc_scl_o = '0' else 'Z';
  fpga_sda_b <= '0' when wrc_sda_o = '0' else 'Z';
  wrc_scl_i  <= fpga_scl_b;
  wrc_sda_i  <= fpga_sda_b;

  sfp_mod_def1_b <= '0' when sfp_scl_o = '0' else 'Z';
  sfp_mod_def2_b <= '0' when sfp_sda_o = '0' else 'Z';
  sfp_scl_i      <= sfp_mod_def1_b;
  sfp_sda_i      <= sfp_mod_def2_b;


  PERIPH: wrc_periph
   generic map(
    g_phys_uart    => false,
    g_virtual_uart => true 
   )
   port map(
     clk_sys_i   => clk_sys,
     rst_n_i     => L_RST_N,

     led_red_o   => LED_RED,
     led_green_o => LED_GREEN,
     scl_o       => wrc_scl_o,
     scl_i       => wrc_scl_i,
     sda_o       => wrc_sda_o,
     sda_i       => wrc_sda_i,
     sfp_scl_o   => sfp_scl_o,
     sfp_scl_i   => sfp_scl_i,
     sfp_sda_o   => sfp_sda_o,
     sfp_sda_i   => sfp_sda_i,
     sfp_det_i   => sfp_mod_def0_b,
     memsize_i   => "0000",
     btn1_i      => button1_i,
     btn2_i      => button2_i,
     spi_sclk_o  => spi_sclk_o,
     spi_ncs_o   => spi_ncs_o,
     spi_mosi_o  => spi_mosi_o,
     spi_miso_i  => spi_miso_i,

     slave_i => periph_slv_in,
     slave_o => periph_slv_out,

     uart_rxd_i => '0',

     owr_i    => (others=>'1')
   );

   periph_slv_in(1).cyc <= '0';
   periph_slv_in(1).stb <= '0';
   periph_slv_in(1).we <= '0';
   periph_slv_in(1).adr <= (others=>'0');
   periph_slv_in(1).sel <= (others=>'0');
   periph_slv_in(1).dat <= (others=>'0');
   periph_slv_in(2).cyc <= '0';
   periph_slv_in(2).stb <= '0';
   periph_slv_in(2).we <= '0';
   periph_slv_in(2).adr <= (others=>'0');
   periph_slv_in(2).sel <= (others=>'0');
   periph_slv_in(2).dat <= (others=>'0');

  ---------------------
  WB_CON: xwb_sdb_crossbar
    generic map(
      g_num_masters => 1,
      g_num_slaves  => 1,
      g_registered  => true,
      g_wraparound  => true,
      g_layout(0)   => c_layout,
      g_sdb_addr    => c_sdb_address
    )
    port map(
      clk_sys_i   => clk_sys,
      rst_n_i     => L_RST_N,
      slave_i(0)  => genum_wb_out,
      slave_o(0)  => genum_wb_in,
      master_i(0) => periph_slv_out(0),
      master_o(0) => periph_slv_in(0)
    );

  sfp_tx_disable_o <= '0';

end rtl;
