-- Do not edit; this file was generated by Cheby using these options:
--  --gen-hdl=spec_template_regs.vhd -i spec_template_regs.cheby

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.wishbone_pkg.all;

entity spec_template_regs is
  port (
    rst_n_i              : in    std_logic;
    clk_i                : in    std_logic;
    wb_cyc_i             : in    std_logic;
    wb_stb_i             : in    std_logic;
    wb_adr_i             : in    std_logic_vector(12 downto 2);
    wb_sel_i             : in    std_logic_vector(3 downto 0);
    wb_we_i              : in    std_logic;
    wb_dat_i             : in    std_logic_vector(31 downto 0);
    wb_ack_o             : out   std_logic;
    wb_err_o             : out   std_logic;
    wb_rty_o             : out   std_logic;
    wb_stall_o           : out   std_logic;
    wb_dat_o             : out   std_logic_vector(31 downto 0);

    -- a ROM containing the carrier metadata
    metadata_addr_o      : out   std_logic_vector(5 downto 2);
    metadata_data_i      : in    std_logic_vector(31 downto 0);
    metadata_data_o      : out   std_logic_vector(31 downto 0);

    -- offset to the application metadata
    csr_app_offset_i     : in    std_logic_vector(31 downto 0);
    csr_resets_global_o  : out   std_logic;
    csr_resets_appl_o    : out   std_logic;

    -- presence lines for the fmcs
    csr_fmc_presence_i   : in    std_logic_vector(31 downto 0);

    -- status of gennum
    csr_gn4124_status_i  : in    std_logic_vector(31 downto 0);

    -- Set when calibration is done.
    csr_ddr_status_calib_done_i : in    std_logic;
    csr_pcb_rev_rev_i    : in    std_logic_vector(3 downto 0);

    -- Thermometer and unique id
    therm_id_i           : in    t_wishbone_master_in;
    therm_id_o           : out   t_wishbone_master_out;

    -- i2c controllers to the fmcs
    fmc_i2c_i            : in    t_wishbone_master_in;
    fmc_i2c_o            : out   t_wishbone_master_out;

    -- spi controller to the flash
    flash_spi_i          : in    t_wishbone_master_in;
    flash_spi_o          : out   t_wishbone_master_out;

    -- dma registers for the gennum core
    dma_i                : in    t_wishbone_master_in;
    dma_o                : out   t_wishbone_master_out;

    -- vector interrupt controller
    vic_i                : in    t_wishbone_master_in;
    vic_o                : out   t_wishbone_master_out;

    -- white-rabbit core registers
    wrc_regs_i           : in    t_wishbone_master_in;
    wrc_regs_o           : out   t_wishbone_master_out
  );
end spec_template_regs;

architecture syn of spec_template_regs is
  signal rd_int                         : std_logic;
  signal wr_int                         : std_logic;
  signal rd_ack_int                     : std_logic;
  signal wr_ack_int                     : std_logic;
  signal wb_en                          : std_logic;
  signal ack_int                        : std_logic;
  signal wb_rip                         : std_logic;
  signal wb_wip                         : std_logic;
  signal metadata_rack                  : std_logic;
  signal metadata_re                    : std_logic;
  signal csr_resets_global_reg          : std_logic;
  signal csr_resets_appl_reg            : std_logic;
  signal therm_id_re                    : std_logic;
  signal therm_id_wt                    : std_logic;
  signal therm_id_rt                    : std_logic;
  signal therm_id_tr                    : std_logic;
  signal therm_id_wack                  : std_logic;
  signal therm_id_rack                  : std_logic;
  signal fmc_i2c_re                     : std_logic;
  signal fmc_i2c_wt                     : std_logic;
  signal fmc_i2c_rt                     : std_logic;
  signal fmc_i2c_tr                     : std_logic;
  signal fmc_i2c_wack                   : std_logic;
  signal fmc_i2c_rack                   : std_logic;
  signal flash_spi_re                   : std_logic;
  signal flash_spi_wt                   : std_logic;
  signal flash_spi_rt                   : std_logic;
  signal flash_spi_tr                   : std_logic;
  signal flash_spi_wack                 : std_logic;
  signal flash_spi_rack                 : std_logic;
  signal dma_re                         : std_logic;
  signal dma_wt                         : std_logic;
  signal dma_rt                         : std_logic;
  signal dma_tr                         : std_logic;
  signal dma_wack                       : std_logic;
  signal dma_rack                       : std_logic;
  signal vic_re                         : std_logic;
  signal vic_wt                         : std_logic;
  signal vic_rt                         : std_logic;
  signal vic_tr                         : std_logic;
  signal vic_wack                       : std_logic;
  signal vic_rack                       : std_logic;
  signal wrc_regs_re                    : std_logic;
  signal wrc_regs_wt                    : std_logic;
  signal wrc_regs_rt                    : std_logic;
  signal wrc_regs_tr                    : std_logic;
  signal wrc_regs_wack                  : std_logic;
  signal wrc_regs_rack                  : std_logic;
  signal reg_rdat_int                   : std_logic_vector(31 downto 0);
  signal rd_ack1_int                    : std_logic;
begin

  -- WB decode signals
  wb_en <= wb_cyc_i and wb_stb_i;

  process (clk_i, rst_n_i) begin
    if rst_n_i = '0' then
      wb_rip <= '0';
    elsif rising_edge(clk_i) then
      wb_rip <= (wb_rip or (wb_en and not wb_we_i)) and not rd_ack_int;
    end if;
  end process;
  rd_int <= (wb_en and not wb_we_i) and not wb_rip;

  process (clk_i, rst_n_i) begin
    if rst_n_i = '0' then
      wb_wip <= '0';
    elsif rising_edge(clk_i) then
      wb_wip <= (wb_wip or (wb_en and wb_we_i)) and not wr_ack_int;
    end if;
  end process;
  wr_int <= (wb_en and wb_we_i) and not wb_wip;

  ack_int <= rd_ack_int or wr_ack_int;
  wb_ack_o <= ack_int;
  wb_stall_o <= not ack_int and wb_en;
  wb_rty_o <= '0';
  wb_err_o <= '0';

  -- Assign outputs
  process (clk_i, rst_n_i) begin
    if rst_n_i = '0' then
      metadata_rack <= '0';
    elsif rising_edge(clk_i) then
      metadata_rack <= metadata_re and not metadata_rack;
    end if;
  end process;
  metadata_data_o <= wb_dat_i;
  metadata_addr_o <= wb_adr_i(5 downto 2);
  csr_resets_global_o <= csr_resets_global_reg;
  csr_resets_appl_o <= csr_resets_appl_reg;

  -- Assignments for submap therm_id
  therm_id_tr <= therm_id_wt or therm_id_rt;
  process (clk_i, rst_n_i) begin
    if rst_n_i = '0' then
      therm_id_rt <= '0';
    elsif rising_edge(clk_i) then
      therm_id_rt <= (therm_id_rt or therm_id_re) and not therm_id_rack;
    end if;
  end process;
  therm_id_o.cyc <= therm_id_tr;
  therm_id_o.stb <= therm_id_tr;
  therm_id_wack <= therm_id_i.ack and therm_id_wt;
  therm_id_rack <= therm_id_i.ack and therm_id_rt;
  therm_id_o.adr <= ((27 downto 0 => '0') & wb_adr_i(3 downto 2)) & (1 downto 0 => '0');
  therm_id_o.sel <= (others => '1');
  therm_id_o.we <= therm_id_wt;
  therm_id_o.dat <= wb_dat_i;

  -- Assignments for submap fmc_i2c
  fmc_i2c_tr <= fmc_i2c_wt or fmc_i2c_rt;
  process (clk_i, rst_n_i) begin
    if rst_n_i = '0' then
      fmc_i2c_rt <= '0';
    elsif rising_edge(clk_i) then
      fmc_i2c_rt <= (fmc_i2c_rt or fmc_i2c_re) and not fmc_i2c_rack;
    end if;
  end process;
  fmc_i2c_o.cyc <= fmc_i2c_tr;
  fmc_i2c_o.stb <= fmc_i2c_tr;
  fmc_i2c_wack <= fmc_i2c_i.ack and fmc_i2c_wt;
  fmc_i2c_rack <= fmc_i2c_i.ack and fmc_i2c_rt;
  fmc_i2c_o.adr <= ((26 downto 0 => '0') & wb_adr_i(4 downto 2)) & (1 downto 0 => '0');
  fmc_i2c_o.sel <= (others => '1');
  fmc_i2c_o.we <= fmc_i2c_wt;
  fmc_i2c_o.dat <= wb_dat_i;

  -- Assignments for submap flash_spi
  flash_spi_tr <= flash_spi_wt or flash_spi_rt;
  process (clk_i, rst_n_i) begin
    if rst_n_i = '0' then
      flash_spi_rt <= '0';
    elsif rising_edge(clk_i) then
      flash_spi_rt <= (flash_spi_rt or flash_spi_re) and not flash_spi_rack;
    end if;
  end process;
  flash_spi_o.cyc <= flash_spi_tr;
  flash_spi_o.stb <= flash_spi_tr;
  flash_spi_wack <= flash_spi_i.ack and flash_spi_wt;
  flash_spi_rack <= flash_spi_i.ack and flash_spi_rt;
  flash_spi_o.adr <= ((26 downto 0 => '0') & wb_adr_i(4 downto 2)) & (1 downto 0 => '0');
  flash_spi_o.sel <= (others => '1');
  flash_spi_o.we <= flash_spi_wt;
  flash_spi_o.dat <= wb_dat_i;

  -- Assignments for submap dma
  dma_tr <= dma_wt or dma_rt;
  process (clk_i, rst_n_i) begin
    if rst_n_i = '0' then
      dma_rt <= '0';
    elsif rising_edge(clk_i) then
      dma_rt <= (dma_rt or dma_re) and not dma_rack;
    end if;
  end process;
  dma_o.cyc <= dma_tr;
  dma_o.stb <= dma_tr;
  dma_wack <= dma_i.ack and dma_wt;
  dma_rack <= dma_i.ack and dma_rt;
  dma_o.adr <= ((25 downto 0 => '0') & wb_adr_i(5 downto 2)) & (1 downto 0 => '0');
  dma_o.sel <= (others => '1');
  dma_o.we <= dma_wt;
  dma_o.dat <= wb_dat_i;

  -- Assignments for submap vic
  vic_tr <= vic_wt or vic_rt;
  process (clk_i, rst_n_i) begin
    if rst_n_i = '0' then
      vic_rt <= '0';
    elsif rising_edge(clk_i) then
      vic_rt <= (vic_rt or vic_re) and not vic_rack;
    end if;
  end process;
  vic_o.cyc <= vic_tr;
  vic_o.stb <= vic_tr;
  vic_wack <= vic_i.ack and vic_wt;
  vic_rack <= vic_i.ack and vic_rt;
  vic_o.adr <= ((23 downto 0 => '0') & wb_adr_i(7 downto 2)) & (1 downto 0 => '0');
  vic_o.sel <= (others => '1');
  vic_o.we <= vic_wt;
  vic_o.dat <= wb_dat_i;

  -- Assignments for submap wrc_regs
  wrc_regs_tr <= wrc_regs_wt or wrc_regs_rt;
  process (clk_i, rst_n_i) begin
    if rst_n_i = '0' then
      wrc_regs_rt <= '0';
    elsif rising_edge(clk_i) then
      wrc_regs_rt <= (wrc_regs_rt or wrc_regs_re) and not wrc_regs_rack;
    end if;
  end process;
  wrc_regs_o.cyc <= wrc_regs_tr;
  wrc_regs_o.stb <= wrc_regs_tr;
  wrc_regs_wack <= wrc_regs_i.ack and wrc_regs_wt;
  wrc_regs_rack <= wrc_regs_i.ack and wrc_regs_rt;
  wrc_regs_o.adr <= ((19 downto 0 => '0') & wb_adr_i(11 downto 2)) & (1 downto 0 => '0');
  wrc_regs_o.sel <= (others => '1');
  wrc_regs_o.we <= wrc_regs_wt;
  wrc_regs_o.dat <= wb_dat_i;

  -- Process for write requests.
  process (clk_i, rst_n_i) begin
    if rst_n_i = '0' then
      wr_ack_int <= '0';
      csr_resets_global_reg <= '0';
      csr_resets_appl_reg <= '0';
      therm_id_wt <= '0';
      fmc_i2c_wt <= '0';
      flash_spi_wt <= '0';
      dma_wt <= '0';
      vic_wt <= '0';
      wrc_regs_wt <= '0';
    elsif rising_edge(clk_i) then
      wr_ack_int <= '0';
      therm_id_wt <= '0';
      fmc_i2c_wt <= '0';
      flash_spi_wt <= '0';
      dma_wt <= '0';
      vic_wt <= '0';
      wrc_regs_wt <= '0';
      case wb_adr_i(12 downto 12) is
      when "0" => 
        case wb_adr_i(11 downto 8) is
        when "0000" => 
          case wb_adr_i(7 downto 6) is
          when "00" => 
            -- Submap metadata
          when "01" => 
            case wb_adr_i(5 downto 4) is
            when "00" => 
              case wb_adr_i(3 downto 2) is
              when "00" => 
                -- Register csr_app_offset
              when "01" => 
                -- Register csr_resets
                if wr_int = '1' then
                  csr_resets_global_reg <= wb_dat_i(0);
                  csr_resets_appl_reg <= wb_dat_i(1);
                end if;
                wr_ack_int <= wr_int;
              when "10" => 
                -- Register csr_fmc_presence
              when "11" => 
                -- Register csr_gn4124_status
              when others =>
                wr_ack_int <= wr_int;
              end case;
            when "01" => 
              case wb_adr_i(3 downto 2) is
              when "00" => 
                -- Register csr_ddr_status
              when "01" => 
                -- Register csr_pcb_rev
              when others =>
                wr_ack_int <= wr_int;
              end case;
            when "11" => 
              -- Submap therm_id
              therm_id_wt <= (therm_id_wt or wr_int) and not therm_id_wack;
              wr_ack_int <= therm_id_wack;
            when others =>
              wr_ack_int <= wr_int;
            end case;
          when "10" => 
            case wb_adr_i(5 downto 5) is
            when "0" => 
              -- Submap fmc_i2c
              fmc_i2c_wt <= (fmc_i2c_wt or wr_int) and not fmc_i2c_wack;
              wr_ack_int <= fmc_i2c_wack;
            when "1" => 
              -- Submap flash_spi
              flash_spi_wt <= (flash_spi_wt or wr_int) and not flash_spi_wack;
              wr_ack_int <= flash_spi_wack;
            when others =>
              wr_ack_int <= wr_int;
            end case;
          when "11" => 
            -- Submap dma
            dma_wt <= (dma_wt or wr_int) and not dma_wack;
            wr_ack_int <= dma_wack;
          when others =>
            wr_ack_int <= wr_int;
          end case;
        when "0001" => 
          -- Submap vic
          vic_wt <= (vic_wt or wr_int) and not vic_wack;
          wr_ack_int <= vic_wack;
        when others =>
          wr_ack_int <= wr_int;
        end case;
      when "1" => 
        -- Submap wrc_regs
        wrc_regs_wt <= (wrc_regs_wt or wr_int) and not wrc_regs_wack;
        wr_ack_int <= wrc_regs_wack;
      when others =>
        wr_ack_int <= wr_int;
      end case;
    end if;
  end process;

  -- Process for registers read.
  process (clk_i, rst_n_i) begin
    if rst_n_i = '0' then
      rd_ack1_int <= '0';
      reg_rdat_int <= (others => 'X');
    elsif rising_edge(clk_i) then
      reg_rdat_int <= (others => '0');
      case wb_adr_i(12 downto 12) is
      when "0" => 
        case wb_adr_i(11 downto 8) is
        when "0000" => 
          case wb_adr_i(7 downto 6) is
          when "00" => 
          when "01" => 
            case wb_adr_i(5 downto 4) is
            when "00" => 
              case wb_adr_i(3 downto 2) is
              when "00" => 
                -- csr_app_offset
                reg_rdat_int <= csr_app_offset_i;
                rd_ack1_int <= rd_int;
              when "01" => 
                -- csr_resets
                reg_rdat_int(0) <= csr_resets_global_reg;
                reg_rdat_int(1) <= csr_resets_appl_reg;
                rd_ack1_int <= rd_int;
              when "10" => 
                -- csr_fmc_presence
                reg_rdat_int <= csr_fmc_presence_i;
                rd_ack1_int <= rd_int;
              when "11" => 
                -- csr_gn4124_status
                reg_rdat_int <= csr_gn4124_status_i;
                rd_ack1_int <= rd_int;
              when others =>
                rd_ack1_int <= rd_int;
              end case;
            when "01" => 
              case wb_adr_i(3 downto 2) is
              when "00" => 
                -- csr_ddr_status
                reg_rdat_int(0) <= csr_ddr_status_calib_done_i;
                rd_ack1_int <= rd_int;
              when "01" => 
                -- csr_pcb_rev
                reg_rdat_int(3 downto 0) <= csr_pcb_rev_rev_i;
                rd_ack1_int <= rd_int;
              when others =>
                rd_ack1_int <= rd_int;
              end case;
            when "11" => 
            when others =>
              rd_ack1_int <= rd_int;
            end case;
          when "10" => 
            case wb_adr_i(5 downto 5) is
            when "0" => 
            when "1" => 
            when others =>
              rd_ack1_int <= rd_int;
            end case;
          when "11" => 
          when others =>
            rd_ack1_int <= rd_int;
          end case;
        when "0001" => 
        when others =>
          rd_ack1_int <= rd_int;
        end case;
      when "1" => 
      when others =>
        rd_ack1_int <= rd_int;
      end case;
    end if;
  end process;

  -- Process for read requests.
  process (wb_adr_i, reg_rdat_int, rd_ack1_int, rd_int, rd_int, metadata_data_i, metadata_rack, rd_int, therm_id_i.dat, therm_id_rack, therm_id_rt, rd_int, fmc_i2c_i.dat, fmc_i2c_rack, fmc_i2c_rt, rd_int, flash_spi_i.dat, flash_spi_rack, flash_spi_rt, rd_int, dma_i.dat, dma_rack, dma_rt, rd_int, vic_i.dat, vic_rack, vic_rt, rd_int, wrc_regs_i.dat, wrc_regs_rack, wrc_regs_rt) begin
    -- By default ack read requests
    wb_dat_o <= (others => '0');
    metadata_re <= '0';
    therm_id_re <= '0';
    fmc_i2c_re <= '0';
    flash_spi_re <= '0';
    dma_re <= '0';
    vic_re <= '0';
    wrc_regs_re <= '0';
    case wb_adr_i(12 downto 12) is
    when "0" => 
      case wb_adr_i(11 downto 8) is
      when "0000" => 
        case wb_adr_i(7 downto 6) is
        when "00" => 
          -- Submap metadata
          wb_dat_o <= metadata_data_i;
          rd_ack_int <= metadata_rack;
          metadata_re <= rd_int;
        when "01" => 
          case wb_adr_i(5 downto 4) is
          when "00" => 
            case wb_adr_i(3 downto 2) is
            when "00" => 
              -- csr_app_offset
              wb_dat_o <= reg_rdat_int;
              rd_ack_int <= rd_ack1_int;
            when "01" => 
              -- csr_resets
              wb_dat_o <= reg_rdat_int;
              rd_ack_int <= rd_ack1_int;
            when "10" => 
              -- csr_fmc_presence
              wb_dat_o <= reg_rdat_int;
              rd_ack_int <= rd_ack1_int;
            when "11" => 
              -- csr_gn4124_status
              wb_dat_o <= reg_rdat_int;
              rd_ack_int <= rd_ack1_int;
            when others =>
              rd_ack_int <= rd_int;
            end case;
          when "01" => 
            case wb_adr_i(3 downto 2) is
            when "00" => 
              -- csr_ddr_status
              wb_dat_o <= reg_rdat_int;
              rd_ack_int <= rd_ack1_int;
            when "01" => 
              -- csr_pcb_rev
              wb_dat_o <= reg_rdat_int;
              rd_ack_int <= rd_ack1_int;
            when others =>
              rd_ack_int <= rd_int;
            end case;
          when "11" => 
            -- Submap therm_id
            therm_id_re <= rd_int;
            wb_dat_o <= therm_id_i.dat;
            rd_ack_int <= therm_id_rack;
          when others =>
            rd_ack_int <= rd_int;
          end case;
        when "10" => 
          case wb_adr_i(5 downto 5) is
          when "0" => 
            -- Submap fmc_i2c
            fmc_i2c_re <= rd_int;
            wb_dat_o <= fmc_i2c_i.dat;
            rd_ack_int <= fmc_i2c_rack;
          when "1" => 
            -- Submap flash_spi
            flash_spi_re <= rd_int;
            wb_dat_o <= flash_spi_i.dat;
            rd_ack_int <= flash_spi_rack;
          when others =>
            rd_ack_int <= rd_int;
          end case;
        when "11" => 
          -- Submap dma
          dma_re <= rd_int;
          wb_dat_o <= dma_i.dat;
          rd_ack_int <= dma_rack;
        when others =>
          rd_ack_int <= rd_int;
        end case;
      when "0001" => 
        -- Submap vic
        vic_re <= rd_int;
        wb_dat_o <= vic_i.dat;
        rd_ack_int <= vic_rack;
      when others =>
        rd_ack_int <= rd_int;
      end case;
    when "1" => 
      -- Submap wrc_regs
      wrc_regs_re <= rd_int;
      wb_dat_o <= wrc_regs_i.dat;
      rd_ack_int <= wrc_regs_rack;
    when others =>
      rd_ack_int <= rd_int;
    end case;
  end process;
end syn;
