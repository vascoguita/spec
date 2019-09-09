.. _spec_hdl_spec_base:

SPEC Base HDL Component
=======================

The ``SPEC base`` HDL component provides the basic support for the SPEC card
and it strongly recommended for any SPEC based application. The VHDL code for
this component is part of the `SPEC project`_ source code as well as the
necessary Linux drivers.

Interface Rules
---------------
The ``SPEC base`` is an :ref:`FPGA device <device-structure>` that contains
all the necessary logic to use the SPEC carrier's features.

Rule
  The ``SPEC base`` design must follow the FPGA design guide lines

Rule
  The ``SPEC base`` instance must be present in any SPEC based
  design.

Rule
  The ``SPEC base`` metadata table must contain the following
  constant values

      ==========  ==========  ==================  ============
      Offset      Size (bit)  Name                Default (LE)
      0x00000000  32          Vendor ID           0x000010DC
      0x00000004  32          Device ID           0x53504543
      0x00000008  32          Version             <variable>
      0x0000000C  32          Byte Order Mark     0xFFFE0000
      0x00000010  128         Source ID           <variable>
      0x00000020  32          Capability Mask     <variable>
      0x00000030  128         Vendor UUID         0x00000000
      ==========  ==========  ==================  ============

Observation
  The ``SPEC base`` typically is instantiated in a *top level* design
  next to an ``Application Device``.

Rule
  The ``SPEC base`` must have a 32bit register containing the offset
  to the ``Application Device``. If there is no application, then the content
  of this register must be ``0x00000000``.

Observation
  The ``Application Device`` offset is design specific and it must be
  declared in the ``Application Access`` register

Version 1.4
~~~~~~~~~~~

Rule
  The ``SPEC base`` metadata table must contain the following
  constant values for this version.

      ==========  ==========  ==================  ============
      Offset      Size (bit)  Name                Default (LE)
      0x00000000  32          Vendor ID           0x000010DC
      0x00000004  32          Device ID           0x53504543
      0x00000008  32          Version             0x0104xxxx
      0x0000000C  32          Byte Order Mark     0xFFFE0000
      0x00000010  128         Source ID           <variable>
      0x00000020  32          Capability Mask     0x0000000x
      0x00000030  128         Vendor UUID         0x00000000
      ==========  ==========  ==================  ============

Rule
  The ``SPEC base`` is made of the following components

     ===================  ============  ==========  =============
     Component            Start         End         Cap. Mask Bit
     CSR                  0x00000040    0x0000005F  (Mandatory)
     Therm. & ID          0x00000070    0x0000007F  1
     Gen-Core I2C Ocore   0x00000080    0x0000009F  (Mandatory)
     Gen-Core SPI         0x000000A0    0x000000BF  2
     DMA for DDR          0x000000C0    0x000000FF  5
     Gen-Core VIC         0x00000100    0x000001FF  0
     Build info           0x00000200    0x000002FF  4
     White-Rabbit         0x00001000    0x00001FFF  3
     ===================  ============  ==========  =============

Observation
  The capability mask value ``0x1F`` means that all optional components
  are instantiated.

Rule
  The ``SPEC base`` must connect the VIC IRQ output to the ``GPIO 8`` on
  the GN4124 chip

Observation
  The GN4124 ``GPIO 9`` can be used for interrupts by the application.

Rule
  The ``SPEC base`` reserves the first 6 interrupt lines of
  the internal interrupt controller (``VIC``) for the following purposes:

  ==============  ===================
  Interrupt Line  Component
  0               Gen-Core I2C Ocore
  1               Gen-Core SPI
  2               Gen-Core Gennum DMA DONE
  3               (reserved)
  4               (reserved)
  5               (reserved)
  ==============  ===================

.. _`SPEC project`: https://ohwr.org/project/spec
