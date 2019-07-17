--------------------------------------------------------------------------------
-- CERN BE-CO-HT
-- SPEC
-- https://ohwr.org/projects/spec
--------------------------------------------------------------------------------
--
-- unit name:   spec_template_wr
--
-- description: SPEC carrier template, with WR.
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
use work.buildinfo_pkg.all;
use work.wr_fabric_pkg.all;
use work.streamers_pkg.all;

library unisim;
use unisim.vcomponents.all;

entity spec_template_wr is
  generic (
    --  If true, instantiate a VIC/ONEWIRE/SPI/WR.
    g_WITH_VIC      : boolean := True;
    g_WITH_ONEWIRE  : boolean := True;
    g_WITH_SPI      : boolean := True;
    g_WITH_WR       : boolean := True;
    g_WITH_DDR      : boolean := True;
    --  Address of the application meta-data.  0 if none.
    g_APP_OFFSET    : std_logic_vector(31 downto 0) := x"0000_0000";
    --  Number of user interrupts
    g_NUM_USER_IRQ  : natural := 1;
    --  WR PTP firmware.
    g_DPRAM_INITF   : string := "../../../../wr-cores/bin/wrpc/wrc_phy8.bram";
    -- Fabric interface selection for WR Core:
    -- plain     = expose WRC fabric interface
    -- streamers = attach WRC streamers to fabric interface
    -- etherbone = attach Etherbone slave to fabric interface
    g_FABRIC_IFACE  : t_board_fabric_iface := plain;
    -- parameters configuration when g_fabric_iface = "streamers" (otherwise ignored)
    g_STREAMERS_OP_MODE  : t_streamers_op_mode  := TX_AND_RX;
    g_TX_STREAMER_PARAMS : t_tx_streamer_params := c_TX_STREAMER_PARAMS_DEFAUT;
    g_RX_STREAMER_PARAMS : t_rx_streamer_params := c_RX_STREAMER_PARAMS_DEFAUT;
    -- Simulation-mode enable parameter. Set by default (synthesis) to 0, and
    -- changed to non-zero in the instantiation of the top level DUT in the testbench.
    -- Its purpose is to reduce some internal counters/timeouts to speed up simulations.
    g_SIMULATION : integer := 0;
    -- Increase information messages during simulation
    g_VERBOSE    : boolean := False
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
    gn_rst_n_i      : in std_logic; -- Reset from GN4124 (RSTOUT18_N)
    -- PCIe to Local [Inbound Data] - RX
    gn_p2l_clk_n_i  : in  std_logic;       -- Receiver Source Synchronous Clock-
    gn_p2l_clk_p_i  : in  std_logic;       -- Receiver Source Synchronous Clock+
    gn_p2l_rdy_o    : out std_logic;       -- Rx Buffer Full Flag
    gn_p2l_dframe_i : in  std_logic;       -- Receive Frame
    gn_p2l_valid_i  : in  std_logic;       -- Receive Data Valid
    gn_p2l_data_i   : in  std_logic_vector(15 downto 0);  -- Parallel receive data
    -- Inbound Buffer Request/Status
    gn_p_wr_req_i   : in  std_logic_vector(1 downto 0);  -- PCIe Write Request
    gn_p_wr_rdy_o   : out std_logic_vector(1 downto 0);  -- PCIe Write Ready
    gn_rx_error_o   : out std_logic;                     -- Receive Error
    -- Local to Parallel [Outbound Data] - TX
    gn_l2p_clk_n_o  : out std_logic;       -- Transmitter Source Synchronous Clock-
    gn_l2p_clk_p_o  : out std_logic;       -- Transmitter Source Synchronous Clock+
    gn_l2p_dframe_o : out std_logic;       -- Transmit Data Frame
    gn_l2p_valid_o  : out std_logic;       -- Transmit Data Valid
    gn_l2p_edb_o    : out std_logic;       -- Packet termination and discard
    gn_l2p_data_o   : out std_logic_vector(15 downto 0);  -- Parallel transmit data
    -- Outbound Buffer Status
    gn_l2p_rdy_i    : in std_logic;                     -- Tx Buffer Full Flag
    gn_l_wr_rdy_i   : in std_logic_vector(1 downto 0);  -- Local-to-PCIe Write
    gn_p_rd_d_rdy_i : in std_logic_vector(1 downto 0);  -- PCIe-to-Local Read Response Data Ready
    gn_tx_error_i   : in std_logic;                     -- Transmit Error
    gn_vc_rdy_i     : in std_logic_vector(1 downto 0);  -- Channel ready
    -- General Purpose Interface
    gn_gpio_b     : inout std_logic_vector(1 downto 0);  -- gn_gpio[0] -> GN4124 GPIO8
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
    led_link_o  : out std_logic;

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
    pll25dac_cs_n_o   : out std_logic; --cs1
    pll20dac_cs_n_o   : out std_logic; --cs2

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

    --  User part

    --  Direct access to the DDR-3
    --  Classic wishbone
    ddr_dma_clk_i   : in  std_logic;
    ddr_dma_rst_n_i : in std_logic;
    ddr_dma_wb_i    : in  t_wishbone_slave_data64_in;
    ddr_dma_wb_o    : out t_wishbone_slave_data64_out;

    -- DDR FIFO empty flag
    ddr_wr_fifo_empty_o : out std_logic;

    --  Clocks and reset.
    clk_sys_62m5_o    : out std_logic;
    rst_sys_62m5_n_o  : out std_logic;
    clk_ref_125m_o    : out std_logic;
    rst_ref_125m_n_o  : out std_logic;

    --  Interrupts
    irq_user_i : in std_logic_vector(g_NUM_USER_IRQ + 5 downto 6) := (others => '0');

    -- WR fabric interface (when g_fabric_iface = "plain")
    wrf_src_o : out t_wrf_source_out;
    wrf_src_i : in  t_wrf_source_in := c_DUMMY_SRC_IN;
    wrf_snk_o : out t_wrf_sink_out;
    wrf_snk_i : in  t_wrf_sink_in   := c_DUMMY_SNK_IN;

    -- WR streamers (when g_fabric_iface = "streamers")
    wrs_tx_data_i  : in  std_logic_vector(g_TX_STREAMER_PARAMS.DATA_WIDTH-1 downto 0) := (others => '0');
    wrs_tx_valid_i : in  std_logic := '0';
    wrs_tx_dreq_o  : out std_logic;
    wrs_tx_last_i  : in  std_logic := '1';
    wrs_tx_flush_i : in  std_logic := '0';
    wrs_tx_cfg_i   : in  t_tx_streamer_cfg := c_TX_STREAMER_CFG_DEFAULT;
    wrs_rx_first_o : out std_logic;
    wrs_rx_last_o  : out std_logic;
    wrs_rx_data_o  : out std_logic_vector(g_rx_streamer_params.data_width-1 downto 0);
    wrs_rx_valid_o : out std_logic;
    wrs_rx_dreq_i  : in  std_logic := '0';
    wrs_rx_cfg_i   : in  t_rx_streamer_cfg := c_RX_STREAMER_CFG_DEFAULT;

    -- Etherbone WB master interface (when g_fabric_iface = "etherbone")
    wb_eth_master_o : out t_wishbone_master_out;
    wb_eth_master_i : in  t_wishbone_master_in := cc_dummy_master_in;

    -- Timecode I/F
    tm_link_up_o    : out std_logic;
    tm_time_valid_o : out std_logic;
    tm_tai_o        : out std_logic_vector(39 downto 0);
    tm_cycles_o     : out std_logic_vector(27 downto 0);

    -- PPS output
    pps_p_o    : out std_logic;
    pps_led_o  : out std_logic;

    -- Link ok indication
    link_ok_o  : out std_logic;

    --  The wishbone bus from the gennum/host to the application
    --  Addresses 0-0x1fff are not available (used by the carrier).
    --  This is a pipelined wishbone with byte granularity.
    app_wb_o           : out t_wishbone_master_out;
    app_wb_i           : in  t_wishbone_master_in
  );
end entity spec_template_wr;

architecture top of spec_template_wr is
  -- WRPC Xilinx platform auxiliary clock configuration, used for DDR clock
  constant c_WRPC_PLL_CONFIG : t_auxpll_cfg_array := (
    0      => (enabled => TRUE, bufg_en => TRUE, divide => 3),
    others => c_AUXPLL_CFG_DEFAULT);

  signal clk_sys_62m5    : std_logic;  -- 62.5Mhz

  signal clk_pll_aux     : std_logic_vector(3 downto 0);
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

  signal gn_wb_out     : t_wishbone_master_out;
  signal gn_wb_in      : t_wishbone_master_in;

  --  The wishbone bus to the carrier part.
  signal carrier_wb_out : t_wishbone_slave_out;
  signal carrier_wb_in  : t_wishbone_slave_in;

  signal gennum_status : std_logic_vector(31 downto 0);

  signal metadata_addr : std_logic_vector(5 downto 2);
  signal metadata_data : std_logic_vector(31 downto 0);

  signal buildinfo_addr : std_logic_vector(7 downto 2);
  signal buildinfo_data : std_logic_vector(31 downto 0);

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

  signal csr_rst_gbl : std_logic;
  signal csr_rst_app : std_logic;

  signal rst_csr_app_n      : std_logic;
  signal rst_csr_app_sync_n : std_logic;

  signal rst_gbl_n : std_logic;

  signal fmc0_scl_out, fmc0_sda_out : std_logic;
  signal fmc0_scl_oen, fmc0_sda_oen : std_logic;

  signal fmc_presence : std_logic_vector(31 downto 0);

  signal irq_master : std_logic;

  constant num_interrupts : natural := 6 + g_NUM_USER_IRQ;
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

  attribute keep                 : string;
  attribute keep of clk_sys_62m5 : signal is "TRUE";
  attribute keep of clk_ref_125m : signal is "TRUE";
  attribute keep of clk_ddr_333m : signal is "TRUE";
  attribute keep of ddr_rst      : signal is "TRUE";

begin  -- architecture top

  ------------------------------------------------------------------------------
  -- GN4124 interface
  ------------------------------------------------------------------------------

  gn_gpio_b(1) <= 'Z';

  cmp_gn4124_core : entity work.xwb_gn4124_core
    generic map (
      g_WITH_DMA                    => g_WITH_DDR,
      g_WBM_TO_WB_FIFO_SIZE         => 16,
      g_WBM_TO_WB_FIFO_FULL_THRES   => 12,
      g_WBM_FROM_WB_FIFO_SIZE       => 16,
      g_WBM_FROM_WB_FIFO_FULL_THRES => 12,
      g_P2L_FIFO_SIZE               => 256,
      g_P2L_FIFO_FULL_THRES         => 175,
      g_L2P_ADDR_FIFO_FULL_SIZE     => 256,
      g_L2P_ADDR_FIFO_FULL_THRES    => 175,
      g_L2P_DATA_FIFO_FULL_SIZE     => 256,
      g_L2P_DATA_FIFO_FULL_THRES    => 175
    )
    port map (
      ---------------------------------------------------------
      -- Control and status
      rst_n_a_i => gn_rst_n_i,
      status_o  => gennum_status,

      ---------------------------------------------------------
      -- P2L Direction
      --
      -- Source Sync DDR related signals
      p2l_clk_p_i  => gn_p2l_clk_p_i,
      p2l_clk_n_i  => gn_p2l_clk_n_i,
      p2l_data_i   => gn_p2l_data_i,
      p2l_dframe_i => gn_p2l_dframe_i,
      p2l_valid_i  => gn_p2l_valid_i,
      -- P2L Control
      p2l_rdy_o    => gn_p2l_rdy_o,
      p_wr_req_i   => gn_p_wr_req_i,
      p_wr_rdy_o   => gn_p_wr_rdy_o,
      rx_error_o   => gn_rx_error_o,
      vc_rdy_i     => gn_vc_rdy_i,

      ---------------------------------------------------------
      -- L2P Direction
      --
      -- Source Sync DDR related signals
      l2p_clk_p_o  => gn_l2p_clk_p_o,
      l2p_clk_n_o  => gn_l2p_clk_n_o,
      l2p_data_o   => gn_l2p_data_o,
      l2p_dframe_o => gn_l2p_dframe_o,
      l2p_valid_o  => gn_l2p_valid_o,
      -- L2P Control
      l2p_edb_o    => gn_l2p_edb_o,
      l2p_rdy_i    => gn_l2p_rdy_i,
      l_wr_rdy_i   => gn_l_wr_rdy_i,
      p_rd_d_rdy_i => gn_p_rd_d_rdy_i,
      tx_error_i   => gn_tx_error_i,

      ---------------------------------------------------------
      -- Interrupt interface
      -- Note: the dma_irq are synchronized with the wb_master_clk clock
      --  inside the gn4124 core.
      dma_irq_o => irqs(3 downto 2),
      -- Note: this is a simple assignment.
      irq_p_i   => irq_master,
      irq_p_o   => gn_gpio_b(0),

      ---------------------------------------------------------
      -- DMA registers wishbone interface (slave classic)
      wb_dma_cfg_clk_i => clk_sys_62m5,
      wb_dma_cfg_rst_n_i => rst_sys_62m5_n,
      wb_dma_cfg_i => dma_out,
      wb_dma_cfg_o => dma_in,

      ---------------------------------------------------------
      -- CSR wishbone interface (master pipelined)
      wb_master_clk_i   => clk_sys_62m5,
      wb_master_rst_n_i => rst_sys_62m5_n,
      wb_master_o       => gn_wb_out,
      wb_master_i       => gn_wb_in,

      ---------------------------------------------------------
      -- L2P DMA Interface (Pipelined Wishbone master)
      wb_dma_dat_clk_i   => clk_sys_62m5,
      wb_dma_dat_rst_n_i => rst_gbl_n,
      wb_dma_dat_o       => gn_wb_ddr_out,
      wb_dma_dat_i       => gn_wb_ddr_in
    );

  --  Mini-crossbar from gennum to carrier and application bus.
  carrier_app_xb: process (clk_sys_62m5)
  is
    type t_ca_state is (S_IDLE, S_APP, S_CARRIER);
    variable ca_state            : t_ca_state;
    variable can_stall           : std_logic;
    constant c_IDLE_WB_MASTER_IN : t_wishbone_master_in :=
      (ack => '0', err => '0', rty => '0', stall => '0', dat => c_DUMMY_WB_DATA);
  begin
    if rising_edge(clk_sys_62m5) then
      if rst_sys_62m5_n = '0' then
        ca_state      := S_IDLE;
        gn_wb_in      <= c_IDLE_WB_MASTER_IN;
        app_wb_o      <= c_DUMMY_WB_MASTER_OUT;
        carrier_wb_in <= c_DUMMY_WB_MASTER_OUT;
      else
        case ca_state is
          when S_IDLE =>
            gn_wb_in      <= c_IDLE_WB_MASTER_IN;
            app_wb_o      <= c_DUMMY_WB_MASTER_OUT;
            carrier_wb_in <= c_DUMMY_WB_MASTER_OUT;
            if gn_wb_out.cyc = '1'
              and gn_wb_out.stb = '1'
            then
              -- New transaction.
              -- Stall so that there is no new requests from the master.
              gn_wb_in.stall <= '1';
              can_stall      := '1';
              if gn_wb_out.adr (31 downto 13) = (31 downto 13 => '0') then
                ca_state := S_CARRIER;
                --  Pass to carrier
                carrier_wb_in <= gn_wb_out;
              else
                ca_state := S_APP;
                app_wb_o <= gn_wb_out;
              end if;
            end if;
          when S_CARRIER =>
            --  Pass from carrier.
            --  Maintain stb as long as the carrier stalls.
            carrier_wb_in.stb <= carrier_wb_out.stall and can_stall;
            can_stall         := can_stall and carrier_wb_out.stall;
            gn_wb_in          <= carrier_wb_out;
            gn_wb_in.stall    <= '1';
            if carrier_wb_out.ack = '1' then
              ca_state := S_IDLE;
            end if;
          when S_APP =>
            --  Pass from application
            app_wb_o.stb   <= app_wb_i.stall and can_stall;
            can_stall      := can_stall and app_wb_i.stall;
            gn_wb_in       <= app_wb_i;
            gn_wb_in.stall <= '1';
            if app_wb_i.ack = '1' or app_wb_i.err = '1' then
              ca_state := S_IDLE;
            end if;
        end case;
      end if;
    end if;
  end process carrier_app_xb;

  i_devs: entity work.spec_template_regs
    port map (
      rst_n_i    => rst_sys_62m5_n,
      clk_i      => clk_sys_62m5,
      wb_cyc_i   => carrier_wb_in.cyc,
      wb_stb_i   => carrier_wb_in.stb,
      wb_adr_i   => carrier_wb_in.adr (12 downto 2),  -- Bytes address from gennum
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
      csr_app_offset_i    => g_APP_OFFSET,
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

      -- a ROM containing build info
      buildinfo_addr_o => buildinfo_addr,
      buildinfo_data_i => buildinfo_data,
      buildinfo_data_o => open,

      -- white-rabbit core
      wrc_regs_i          => wrc_in,
      wrc_regs_o          => wrc_out
    );

  --  Metadata
  p_metadata: process (clk_sys_62m5) is
  begin
    if rising_edge(clk_sys_62m5) then
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
          if g_WITH_VIC then
            metadata_data(0) <= '1';
          end if;
          if g_WITH_ONEWIRE and not g_WITH_WR then
            metadata_data(1) <= '1';
          end if;
          if g_WITH_SPI and not g_WITH_WR then
            metadata_data(2) <= '1';
          end if;
          if g_WITH_WR then
            metadata_data(3) <= '1';
          end if;
          if g_WITH_DDR then
            metadata_data(4) <= '1';
          end if;
          --  Buildinfo
          metadata_data(5) <= '1';
        when others =>
          metadata_data <= x"00000000";
      end case;
    end if;
  end process;

  --  Build information
  p_buildinfo: process (clk_sys_62m5) is
    variable addr : natural;
    variable b : std_logic_vector(7 downto 0);
  begin
    if rising_edge(clk_sys_62m5) then
      addr := to_integer(unsigned(buildinfo_addr)) * 4;
      for i in 0 to 3 loop
        if addr + i < buildinfo'length then
           b := std_logic_vector(to_unsigned(character'pos(
             buildinfo(buildinfo'left + addr + i)), 8));
        else
           b := x"00";
        end if;
        buildinfo_data (7 + i * 8 downto i * 8) <= b;
      end loop;
    end if;
  end process;

  fmc_presence (0) <= not fmc0_prsnt_m2c_n_i;
  fmc_presence (31 downto 1) <= (others => '0');

  rst_gbl_n <= rst_sys_62m5_n and (not csr_rst_gbl);

  -- reset for DDR including soft reset.
  -- This is treated as async and will be re-synced by the DDR controller
  ddr_rst <= not rst_ddr_333m_n or csr_rst_gbl;

  rst_csr_app_n <= not (csr_rst_gbl or csr_rst_app);

  rst_sys_62m5_n_o <= rst_sys_62m5_n and rst_csr_app_n;
  clk_sys_62m5_o <= clk_sys_62m5;

  i_rst_csr_app_sync : gc_sync_ffs
    port map (
      clk_i    => clk_ref_125m,
      rst_n_i  => '1',
      data_i   => rst_csr_app_n,
      synced_o => rst_csr_app_sync_n);

  rst_ref_125m_n_o <= rst_ref_125m_n and rst_csr_app_sync_n;
  clk_ref_125m_o   <= clk_ref_125m;

  i_i2c: entity work.xwb_i2c_master
    generic map (
      g_interface_mode      => CLASSIC,
      g_address_granularity => BYTE,
      g_num_interfaces      => 1)
    port map (
      clk_sys_i => clk_sys_62m5,
      rst_n_i   => rst_gbl_n,

      slave_i => fmc_i2c_out,
      slave_o => fmc_i2c_in,
      desc_o  => open,

      int_o   => irqs(0),

      scl_pad_i (0)    => fmc0_scl_b,
      scl_pad_o (0)    => fmc0_scl_out,
      scl_padoen_o (0) => fmc0_scl_oen,
      sda_pad_i (0)    => fmc0_sda_b,
      sda_pad_o (0)    => fmc0_sda_out,
      sda_padoen_o (0) => fmc0_sda_oen
    );

  fmc0_scl_b <= fmc0_scl_out when fmc0_scl_oen = '0' else 'Z';
  fmc0_sda_b <= fmc0_sda_out when fmc0_sda_oen = '0' else 'Z';

  gen_user_irq: if g_NUM_USER_IRQ > 0 generate
    irqs(irq_user_i'range) <= irq_user_i;
  end generate gen_user_irq;

  g_vic: if g_with_vic generate
    i_vic: entity work.xwb_vic
      generic map (
        g_address_granularity => BYTE,
        g_num_interrupts => num_interrupts
      )
      port map (
        clk_sys_i => clk_sys_62m5,
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
  irqs(1) <= '0';
  irqs(4) <= '0';
  irqs(5) <= '0';

  -----------------------------------------------------------------------------
  -- The WR PTP core board package (WB Slave + WB Master #2 (Etherbone))
  -----------------------------------------------------------------------------

  wrc_out_sh <= (cyc => wrc_out.cyc, stb => wrc_out.stb,
                 adr => wrc_out.adr or x"00020000",
                 sel => wrc_out.sel, we => wrc_out.we, dat => wrc_out.dat);

  cmp_xwrc_board_spec : xwrc_board_spec
    generic map (
      g_simulation                => g_SIMULATION,
      g_VERBOSE                   => g_VERBOSE,
      g_with_external_clock_input => TRUE,
      g_dpram_initf               => g_DPRAM_INITF,
      g_AUX_PLL_CFG               => c_WRPC_PLL_CONFIG,
      g_STREAMERS_OP_MODE         => g_STREAMERS_OP_MODE,
      g_TX_STREAMER_PARAMS        => g_TX_STREAMER_PARAMS,
      g_RX_STREAMER_PARAMS        => g_RX_STREAMER_PARAMS,
      g_FABRIC_IFACE              => g_FABRIC_IFACE)
    port map (
      areset_n_i          => button1_i,
      areset_edge_n_i     => gn_rst_n_i,
      clk_20m_vcxo_i      => clk_20m_vcxo_i,
      clk_125m_pllref_p_i => clk_125m_pllref_p_i,
      clk_125m_pllref_n_i => clk_125m_pllref_n_i,
      clk_125m_gtp_n_i    => clk_125m_gtp_n_i,
      clk_125m_gtp_p_i    => clk_125m_gtp_p_i,
      clk_10m_ext_i       => clk_ext_10m,

      clk_sys_62m5_o      => clk_sys_62m5,
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

      wrf_src_o           => wrf_src_o,
      wrf_src_i           => wrf_src_i,
      wrf_snk_o           => wrf_snk_o,
      wrf_snk_i           => wrf_snk_i,
      wrs_tx_data_i       => wrs_tx_data_i,
      wrs_tx_valid_i      => wrs_tx_valid_i,
      wrs_tx_dreq_o       => wrs_tx_dreq_o,
      wrs_tx_last_i       => wrs_tx_last_i,
      wrs_tx_flush_i      => wrs_tx_flush_i,
      wrs_tx_cfg_i        => wrs_tx_cfg_i,
      wrs_rx_first_o      => wrs_rx_first_o,
      wrs_rx_last_o       => wrs_rx_last_o,
      wrs_rx_data_o       => wrs_rx_data_o,
      wrs_rx_valid_o      => wrs_rx_valid_o,
      wrs_rx_dreq_i       => wrs_rx_dreq_i,
      wrs_rx_cfg_i        => wrs_rx_cfg_i,
      wb_eth_master_o     => wb_eth_master_o,
      wb_eth_master_i     => wb_eth_master_i,

      abscal_txts_o       => wrc_abscal_txts_out,
      abscal_rxts_o       => wrc_abscal_rxts_out,

      tm_link_up_o        => tm_link_up_o,
      tm_time_valid_o     => tm_time_valid_o,
      tm_tai_o            => tm_tai_o,
      tm_cycles_o         => tm_cycles_o,

      pps_p_o             => pps_p_o,
      pps_led_o           => pps_led_o,
      link_ok_o           => link_ok_o,
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

  gen_with_ddr: if g_WITH_DDR generate

    --  DDR3 controller
    cmp_ddr_ctrl_bank3 : entity work.ddr3_ctrl
      generic map(
        g_RST_ACT_LOW        => 0, -- active high reset (simpler internal logic)
        g_BANK_PORT_SELECT   => "SPEC_BANK3_64B_32B",
        g_MEMCLK_PERIOD      => 3000,
        g_SIMULATION         => boolean'image(g_SIMULATION /= 0),
        g_CALIB_SOFT_IP      => "TRUE",
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
        p0_wr_empty_o    => ddr_wr_fifo_empty_o,
        p0_wr_count_o    => open,
        p0_wr_underrun_o => open,
        p0_wr_error_o    => open,

        wb1_rst_n_i => rst_gbl_n,
        wb1_clk_i   => clk_sys_62m5,
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

    ddr_calib_done <= ddr_status(0);

    -- unused Wishbone signals
    gn_wb_ddr_in.err <= '0';
    gn_wb_ddr_in.rty <= '0';
  end generate gen_with_ddr;

  gen_without_ddr : if not g_WITH_DDR generate
    ddr_calib_done      <= '0';
    gn_wb_ddr_in        <= c_DUMMY_WB_MASTER_IN;
    ddr_a_o             <= (others => '0');
    ddr_ba_o            <= (others => '0');
    ddr_dq_b            <= (others => 'Z');
    ddr_cas_n_o         <= '0';
    ddr_ck_p_o          <= '0';
    ddr_ck_n_o          <= '0';
    ddr_cke_o           <= '0';
    ddr_ldm_o           <= '0';
    ddr_ldqs_n_b        <= 'Z';
    ddr_ldqs_p_b        <= 'Z';
    ddr_udqs_n_b        <= 'Z';
    ddr_udqs_p_b        <= 'Z';
    ddr_odt_o           <= '0';
    ddr_udm_o           <= '0';
    ddr_ras_n_o         <= '0';
    ddr_reset_n_o       <= '0';
    ddr_we_n_o          <= '0';
    ddr_rzq_b           <= 'Z';
    ddr_dma_wb_o.dat    <= (others => '0');
    ddr_dma_wb_o.ack    <= '1';
    ddr_dma_wb_o.stall  <= '0';
    ddr_wr_fifo_empty_o <= '0';
  end generate gen_without_ddr;

  ddr_dma_wb_o.err <= '0';
  ddr_dma_wb_o.rty <= '0';

end architecture top;
