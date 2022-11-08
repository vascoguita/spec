..
  SPDX-License-Identifier: CC-BY-SA-4.0
  SPDX-FileCopyrightText: 2019-2020 CERN


.. _spec_hdl_spec_base:

SPEC Base HDL Component
=======================

The ``SPEC base`` HDL component provides the basic support for the SPEC card
and it is strongly recommended for any SPEC based design, even though it
is not mandatory.  This component groups together a set of ip-cores which
are required to drive hardware chips and FPGA ip-cores that are handy to
develop SPEC based designs.

The ``SPEC base`` is compliant with the `FPGA device identification`_ rules.

Components
----------

The following table summarizes the ``SPEC base`` components  and after that
you have a brief description of each of them.  We do not expect to add or
remove components in the future so this should be an exhaustive list.

     ===================  ============  ==========  =============
     Component            Start         End         Cap. Mask Bit
     CSR                  0x00000040    0x0000005F  (Mandatory)
     Therm. & ID          0x00000070    0x0000007F  1
     Gen-Core I2C Ocore   0x00000080    0x0000009F  (Mandatory)
     Gen-Core SPI Ocore   0x000000A0    0x000000BF  2
     DMA for DDR          0x000000C0    0x000000FF  5
     Gen-Core VIC         0x00000100    0x000001FF  0
     Build info           0x00000200    0x000002FF  4
     White-Rabbit         0x00001000    0x00001FFF  3
     ===================  ============  ==========  =============

.. note::
   The *Capability Mask Bit* (Cap. Mask Bit) refers to the bit in the
   capability mask described in the `FPGA device identification`_
   rules.

CSR
  Control and Status register for the ``SPEC base`` device.

Therm. & ID
  A onewire interface from `general cores`_ that accesses the SPEC
  thermometer to get temperature and serial number.

General Cores I2C OpenCore
  An I2C controller from `general cores`_ which bus is wired to the FMC
  connector to access the I2C EEPROM on the FMC module.

General Cores SPI OpenCore
  An SPI controller from `general cores`_ which bus is wired to the SPI
  flash memory on which we store FPGA configurations.

DMA for DDR
  A DMA engine from `GN4124 core`_.

General Cores VIC
  An interrupt controller from `general cores`_ that routes FPGA
  interrupts to PCIe bridge (``GPIO 8`` on the GN4124 chip). The interrupt
  lines from 0 to 5 are reserved for internal use as described in the
  following table. All other lines are available for users.

    ==============  ===================
    Interrupt Line  Component
    0               Gen-Core I2C Ocore
    1               Gen-Core SPI Ocore
    2               DMA for DDR - DONE
    3               (reserved)
    4               (reserved)
    5               (reserved)
    ==============  ===================

Build Info
  Free format information (ASCII) about the FPGA synthesis.

White-Rabbit
  The `White-Rabbit core`_.

.. note::
  If the `White-Rabbit core`_ is instantiated then the components
  *Therm. & ID* and *General Cores SPI OpenCore* get disabled because
  they are incompatible.  This because the `White-Rabbit core`_ needs
  the OneWire bus and the SPI bus for internal use, therefore those
  resources can't be used.

Usage
-----

The ``SPEC base`` component is in ``hdl/rtl/spec_base_wr.vhd`` and
examples of its usage are available in ``hdl/top/``.

Remember that the Linux driver expects the ``SPEC base`` at offset
``0x00000000``.

Meta-Data ROM
-------------

These are the fixed fields in the current (|version|) release.

  ==========  ==========  ==================  ============
  Offset      Size (bit)  Name                Default (LE)
  0x00000000  32          Vendor ID           0x000010DC
  0x00000004  32          Device ID           0x53504543
  0x00000008  32          Version             0x0201xxxx
  0x0000000C  32          Byte Order Mark     0xFFFE0000
  0x00000010  128         Source ID           <variable>
  0x00000020  32          Capability Mask     0x0000000x
  0x00000030  128         Vendor UUID         0x00000000
  ==========  ==========  ==================  ============

Memory Map
----------

.. include:: spec_base_regs.rst

.. _`SPEC project`: https://ohwr.org/project/spec
.. _`FPGA device identification`: https://www.ohwr.org/project/fpga-dev-id/
.. _`general cores`: https://www.ohwr.org/projects/general-cores
.. _`GN4124 core`: https://www.ohwr.org/project/gn4124-core/
.. _`White-Rabbit core`: https://ohwr.org/project/wr-cores
