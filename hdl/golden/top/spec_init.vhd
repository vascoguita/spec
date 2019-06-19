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
  generic (
    --  If true, instantiate a VIC
    g_with_vic : boolean := True;
    g_with_onewire : boolean := True
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
--      LED_RED   : out std_logic;
--      LED_GREEN : out std_logic;

      -- I2C to the FMC
      fmc0_scl_b : inout std_logic;
      fmc0_sda_b : inout std_logic;

      --  FMC presence  (there is a pull-up)
      fmc0_prsnt_m2c_n_i: in std_logic;

      onewire_b : inout std_logic;
  
--      button1_i : in std_logic;
--      button2_i : in std_logic;

--      spi_sclk_o : out std_logic; 
--      spi_ncs_o  : out std_logic;
--      spi_mosi_o : out std_logic;
--      spi_miso_i : in  std_logic;

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
  -- Signals declaration
  ------------------------------------------------------------------------------

  signal pllout_clk_sys       : std_logic;
  signal pllout_clk_fb_pllref : std_logic;

  signal clk_125m_pllref  : std_logic;
  signal clk_sys          : std_logic;

  signal genum_wb_out : t_wishbone_master_out;
  signal genum_wb_in  : t_wishbone_master_in;
  
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

  -- the application
  signal app_in               : t_wishbone_master_in;
  signal app_out              : t_wishbone_master_out;

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
      dma_irq_o => irqs(1 downto 0),
      irq_p_i   => irq_master,
      irq_p_o   => GPIO(0),

      ---------------------------------------------------------
      -- DMA registers wishbone interface (slave classic)
      dma_reg_clk_i => clk_sys,
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
      csr_adr_o   => genum_wb_out.adr,
      csr_dat_o   => genum_wb_out.dat,
      csr_sel_o   => genum_wb_out.sel,
      csr_stb_o   => genum_wb_out.stb,
      csr_we_o    => genum_wb_out.we,
      csr_cyc_o   => genum_wb_out.cyc,
      csr_dat_i   => genum_wb_in.dat,
      csr_ack_i   => genum_wb_in.ack,
      csr_stall_i => genum_wb_in.stall,
      csr_err_i   => genum_wb_in.err,
      csr_rty_i   => genum_wb_in.rty,
      csr_int_i   => '0', --  Not used

      ---------------------------------------------------------
      -- L2P DMA Interface (Pipelined Wishbone master)
      dma_clk_i => clk_sys,
      dma_dat_i => (others=>'0'),
      dma_ack_i => '1',
      dma_stall_i => '0',
      dma_err_i => '0',
      dma_rty_i => '0',
      dma_int_i => '0');

  i_devs: entity work.golden_devices
    port map (
      rst_n_i  => L_RST_N,
      clk_i    => clk_sys,
      wb_cyc_i => genum_wb_out.cyc,
      wb_stb_i => genum_wb_out.stb,
      wb_adr_i => genum_wb_out.adr (7 downto 0),  -- Word address from gennum
      wb_sel_i => genum_wb_out.sel,
      wb_we_i  => genum_wb_out.we,
      wb_dat_i => genum_wb_out.dat,
      wb_ack_o => genum_wb_in.ack,
      wb_err_o => genum_wb_in.err,
      wb_rty_o => genum_wb_in.rty,
      wb_stall_o => genum_wb_in.stall,
      wb_dat_o   => genum_wb_in.dat,
  
      -- a ROM containing the carrier metadata
      metadata_addr_o => metadata_addr,
      metadata_data_i => metadata_data,
      metadata_data_o => open,
  
      -- offset to the application metadata
      app_offset_i        => x"0000_0000",
      csr_resets_global_o => csr_rst_gbl,
      csr_resets_appl_o   => csr_rst_app,
  
      -- presence lines for the fmcs
      csr_fmc_presence_i  => fmc_presence,
  
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
      wrc_i               => wrc_in,
      wrc_o               => wrc_out,
  
      -- the application
      app_i               => app_in,
      app_o               => app_out
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
          if g_with_onewire then
            metadata_data(1) <= '1';
          end if;
        when others =>
          metadata_data <= x"00000000";
      end case;
    end if;
  end process;

  fmc_presence (0) <= not fmc0_prsnt_m2c_n_i;
  fmc_presence (31 downto 1) <= (others => '0');
  
  rst_gbl_n <= l_rst_n and (not csr_rst_gbl);
  rst_app_n <= rst_gbl_n and (not csr_rst_app);

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
  
  g_onewire: if g_with_onewire generate
    i_onewire: entity work.xwb_ds182x_readout
      generic map (
      g_CLOCK_FREQ_KHZ => 62_500,
      g_USE_INTERNAL_PPS => True)
    port map (
      clk_i => clk_sys,
      rst_n_i => rst_gbl_n,
  
      wb_i => therm_id_out,
      wb_o => therm_id_in,
  
      pps_p_i => '0',
      onewire_b => onewire_b
    );
  end generate;

  g_no_onewire: if not g_with_onewire generate
    therm_id_in <= (ack => '1', err => '0', rty => '0', stall => '0', dat => x"00000000");
    onewire_b <= 'Z';
  end generate;

  sfp_mod_def1_b <= 'Z';
  sfp_mod_def2_b <= 'Z';
  sfp_tx_disable_o <= '0';

  --  Not used.
  flash_spi_in <= (ack => '1', err => '0', rty => '0', stall => '0', dat => x"00000000");
  wrc_in <= (ack => '1', err => '0', rty => '0', stall => '0', dat => x"00000000");
  app_in <= (ack => '1', err => '0', rty => '0', stall => '0', dat => x"00000000");
end rtl;
