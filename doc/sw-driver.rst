SPEC Driver(s)
==============

There are drivers for the GN4124 chip and there are drivers for the
:ref:`SPEC base<spec_hdl_spec_base>` component. All these drivers are
managed by:

SPEC FMC Carrier
  This is the driver that wrap up all the physical components and the
  :ref:`SPEC base<spec_hdl_spec_base>` ones. It configures the card so
  that all components cooperate correctly.

The driver for the GN4124 chip are always present and distributed as
part of the SPEC driver. They must work no matter what FPGA design has
been loaded on FPGA.

GN4124 GPIO
  This driver provides support for the GN4124 GPIOs. It uses the standard
  Linux `GPIO interface`_ and it export a dedicated IRQ domain.

Gn4124 FCL
  This driver provides support for the GN4124 FCL (FPGA Configuration Loader).
  It uses the `FPGA manager interface`_ to program the FPGA at runtime.

If the SPEC based application is using the :ref:`SPEC
base<spec_hdl_spec_base>` component then it can profit from the
following driver. They are not all mandatory, it depends on the
application, and most of them are distributed separately:

SPEC GN412x DMA
  This driver provides for DMA transfers to/from the SPEC DDR. It uses
  the standard Linux `DMA Engine`_. It is part of the `SPEC project`_

I2C OCORE
  This is the driver for the I2C OCORE IP-core. It is used to communicate with
  the standard FMC EEPROM available what on FMC modules. The driver is
  available in Linux.

SPI OCORE
  This is the driver for the SPI OCORE IP-core. It is used to communicate with
  the M25P32 FLASH memory where FPGA bitstreams are stored. The driver is
  distributed separately.

VIC
  The driver for the VIC interrupt controller IP-core. The driver is
  distributed separately.

.. _`SPEC project`: https://ohwr.org/project/spec
.. _`GPIO interface`: https://www.kernel.org/doc/html/latest/driver-api/gpio/index.html
.. _`FPGA manager interface`: https://www.kernel.org/doc/html/latest/driver-api/fpga/index.html
.. _`DMA Engine`: https://www.kernel.org/doc/html/latest/driver-api/dmaengine/index.html~
