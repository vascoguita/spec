..
  SPDX-License-Identifier: CC-BY-SA-4.0
  SPDX-FileCopyrightText: 2019-2020 CERN

.. _spec_driver:

SPEC Driver(s)
==============

Driver(s) Structure
-------------------

There are drivers for the GN4124 chip and there are drivers for the
:ref:`SPEC base<spec_hdl_spec_base>` component.

.. _spec_fmc_carrier:

SPEC FMC Carrier
  This is the driver that wrap up all the physical components and the
  :ref:`SPEC base<spec_hdl_spec_base>` ones. It configures the card so
  that all components cooperate correctly.

The driver for the GN4124 chip are always present and distributed as
part of the SPEC driver. They must work no matter what FPGA design has
been loaded on FPGA.

.. _gn4124_gpio:

GN4124 GPIO
  This driver provides support for the GN4124 GPIOs. It uses the standard
  Linux `GPIO interface`_ and it export a dedicated IRQ domain.

.. _gn4124_fcl:

GN4124 FCL
  This driver provides support for the GN4124 FCL (FPGA Configuration Loader).
  It uses the `FPGA manager interface`_ to program the FPGA at run-time.

If the SPEC based application is using the :ref:`SPEC
base<spec_hdl_spec_base>` component then it can profit from the
following driver. They are not all mandatory, it depends on the
application, and most of them are distributed separately:

.. _gn4124_fpga_dma:

SPEC GN412x DMA
  This driver provides for DMA transfers to/from the SPEC DDR. It uses
  the standard Linux `DMA Engine`_. It is part of the `SPEC project`_

.. _i2c_ocore:

I2C OCORE
  This is the driver for the I2C OCORE IP-core. It is used to communicate with
  the standard FMC EEPROM available what on FMC modules. The driver is
  available in Linux but also (as a backport) in `general cores`_.

.. _spi_ocore:

SPI OCORE
  This is the driver for the SPI OCORE IP-core. It is used to communicate with
  the M25P32 FLASH memory where FPGA bitstreams are stored. The driver is
  distributed separately in `general cores`_.

.. _vic:

VIC
  The driver for the VIC interrupt controller IP-core. The driver is
  distributed separately in `general cores`_.

.. _`OHWR`: https://ohwr.org
.. _`SPEC project`: https://ohwr.org/project/spec
.. _`FMC`: https://www.ohwr.org/projects/fmc-sw
.. _`GPIO interface`: https://www.kernel.org/doc/html/latest/driver-api/gpio/index.html
.. _`FPGA manager interface`: https://www.kernel.org/doc/html/latest/driver-api/fpga/index.html
.. _`DMA Engine`: https://www.kernel.org/doc/html/latest/driver-api/dmaengine/index.html
.. _`general cores`: https://www.ohwr.org/projects/general-cores

Drivers(s) Build and Install
----------------------------

From the project top level directory, you can find the driver(s) source files
under ``software/kernel``.

The SPEC software uses plain ``Makefile`` to build drivers. Therefore, you can
build the driver by executing ``make``.  To successfully build the SPEC driver
you need to install the `cheby`_ tool that will generate on fly part of the
code for the :ref:`SPEC base<spec_hdl_spec_base>`.  If you do not want to
install `cheby`_ you can define the path to it with the environment
variable ``CHEBY``.  Following an example on how to build the driver(s).::

  # define CHEBY only if it is not installed
  export CHEBY=/path/to/cheby/proto/cheby.py
  cd /path/to/spec/
  make -C software/kernel modules
  make -C software/kernel modules_install

This will build and install  4 drivers:

- :ref:`spec-fmc-carrier.ko<spec_fmc_carrier>`,
- :ref:`gn412x-gpio.ko<gn4124_gpio>`,
- :ref:`gn412x-fcl.ko<gn4124_fcl>`,
- :ref:`spec-gn412x-dma.ko <gn4124_fpga_dma>`.

::

  find software -name "*.ko"
  software/kernel/gn412x-fcl.ko
  software/kernel/gn412x-gpio.ko
  software/kernel/spec-fmc-carrier.ko
  software/kernel/spec-gn412x-dma.ko

Please note that this will not install all soft dependencies which are
distributed separately (:ref:`I2C OpenCore<i2c_ocore>`,
:ref:`SPI OpenCore<spi_ocore>`, :ref:`HT Vector Interrupt Controller<vic>`,
`FMC`_).

.. _`cheby`: https://gitlab.cern.ch/cohtdrivers/cheby

Driver(s) Loading
-----------------

When the card is plugged and the driver(s) installed, the Linux kernel will
load automatically all necessary drivers.

If you need to manually install/remove the driver and its dependencies, you
can use `modprobe(8)`_.::

  sudo modprobe spec-fmc-carrier

If you did not install the drivers you can use `insmod(8)`_ and `rmmod(8)`_.
In this case is useful to know what drivers to load (dependencies) and their
(un)loading order.::

  # typically part of the distribution
  modprobe at24
  modprobe mtd
  modprobe m25p80
  # from OHWR
  insmod /path/to/fmc-sw/drivers/fmc/fmc.ko
  insmod /path/to/general-cores/software/htvic/drivers/htvic.ko
  insmod /path/to/general-cores/software/i2c-ocores/drivers/i2c/busses/i2c-ocores.ko
  insmod /path/to/general-cores/software/spi-ocores/drivers/spi/spi-ocores.ko
  insmod /path/to/spec/software/kernel/gn412x-fcl.ko
  insmod /path/to/spec/software/kernel/gn412x-gpio.ko
  insmod /path/to/spec/software/kernel/spec-gn412x-dma.ko
  # Actually the order above does not really matter, what matters
  # it is that spec-fmc-carrier.ko is loaded as last
  insmod /path/to/spec/software/kernel/spec-fmc-carrier.ko

.. _`modprobe(8)`: https://linux.die.net/man/8/modprobe
.. _`insmod(8)`: https://linux.die.net/man/8/insmod
.. _`rmmod(8)`: https://linux.die.net/man/8/rmmod


Attributes From *sysfs*
-----------------------

In addition to standard *sysfs* attributes for PCI, `DMA Engine`_,
`FPGA manager`_, `GPIO`_, and `FMC`_ there more SPEC specific *sysfs*
attributes.  Here we focus only on those.

At PCI device top-level we can see the `DMA Engine`_ interface and the
GN412x sub-devices for :ref:`GPIO<gn4124_gpio>` and :ref:`FCL<gn4124_fcl>`.
Still at the PCI device top-level there is the directory ``fpga-options``
that contains additional attributes to control the FPGA.

``fpga-options/bootselect`` [R/W]
  It selects (returns) the FPGA access mode. Possible values are:

  - fpga-flash: (default) the FPGA has access to the SPI flash, it uses it
    to load the pre-programmed FPGA configuration;
  - gn4124-fpga: the FPGA is accessible from the PCI bridge, it is used to
    dynamically load an FPGA configuration;
  - gn4124-flash: the SPI flash is accessible form the PCI bridge, it is used
    to load an FPGA configuration on the SPI flash

``fpga-options/load_golden_fpga`` [W]
  It loads the SPEC golden FPGA (if installed). Just write '1' to this file.

If the FPGA is correctly programmed (an FPGA configuration that uses the
:ref:`SPEC base<spec_hdl_spec_base>`) then there will be a directory named
``spec-<pci-id>`` that contains the reference to all FPGA sub-devices and the
following *sysfs* attributes.

``spec-<pci-id>/application_offset`` [R]
  It shows the relative offset (from FPGA base address - resource0) to the
  user application loaded.

``spec-<pci-id>/pcb_rev`` [R]
  It shows the SPEC carrier PCB revision number.

``spec-<pci-id>/reset_app`` [R/W]
  It puts in *reset* (1) or *unreset* (0) the user application.

.. _`GPIO`: https://www.kernel.org/doc/html/latest/driver-api/gpio/index.html
.. _`FPGA manager`: https://www.kernel.org/doc/html/latest/driver-api/fpga/index.html

Attributes From *debugfs*
-------------------------

In addition to standard *debugfs* attributes for PCI, `DMA Engine`_,
`FPGA manager`_, `GPIO`_, and `FMC`_ there more SPEC specific *debugfs*
attributes.  Here we focus only on those.

``gn412x-gpio.<ID>.auto/regs`` [R]
  It dumps the GN412X registers controlling the GPIO module.

``gn412x-fcl.<ID>.auto/regs`` [R]
  It dumps the GN412X registers controlling the FCL module.

``spec-gn412x-dma.<ID>.auto/regs`` [R]
  It dumps the GN412X DMA FPGA registers controlling the DMA ip-core.

``<pci-id>/fpga_device_metadata`` [R]
  It dumps the FPGA device metadata information for the
  :ref:`SPEC base<spec_hdl_spec_base>` and, when it exists, the user
  application one.

``<pci-id>/info`` [R]
  Miscellaneous information about the card status: IRQ mapping.

``<pci-id>/fpga_firmware`` [W]
  It configures the FPGA with a bitstream which name is provided as input.
  Remember that firmwares are installed in ``/lib/firmware`` and alternatively
  you can provide your own path by setting it in
  ``/sys/module/firmware_class/parameters/path``.

``<pci-id>/spec-<pci-id>/csr_regs`` [R]
  It dumps the Control/Status register for
  the :ref:`SPEC base<spec_hdl_spec_base>`

``<pci-id>/spec-<pci-id>/build_info`` [R]
  It shows the FPGA configuration synthesis information

``<pci-id>/spec-<pci-id>/dma`` [RW]
  It exports DMA capabilities to user-space. The user can ``open(2)``
  and ``close(2)`` to request and release a DMA engine channel. Then,
  the user can use ``lseek(2)`` to set the offset in the DDR, and
  ``read(2)``/``write(2)`` to start the DMA transfer.

Module Parameters
-----------------

``version_ignore`` [R]
  When set to 1 (enable) at ``insmod(2)`` time, it forces the driver
  to ignore the version declared in the FPGA bitstream. Particularly
  usefull during development or debugging across major or minor
  version. By default it is set to 0 (disable).

``user_dma_coherent_size`` [RW]
  It sets the maximum size for a coherent DMA memory allocation. A
  change to this value is applied on ``open(2)``
  (file ``<pci-id>/spec-<pci-id>/dma``).

``user_dma_max_segment`` [RW]
  It sets the maximum size for a DMA transfer in a scatterlist. A
  change to this value is applied on the next ``read(2)`` or ``write(2)``
  (file ``<pci-id>/spec-<pci-id>/dma``).

DMA
---

On SPEC-Based designs the DMA engine is implemented in HDL. This means
that you can't perform a DMA transfer without a *spec-base* device
on the FPGA.

The SPEC driver(s) implements the dmaengine API for the HDL DMA
engine. To request a dmaengine channel the user must provide a filter
function. The SPEC driver assigns to the application driver a
IORESOURCE_DMA which value is ``dma_device->dev_id << 16 |
channel_number``. Therefore, the user can use the following filter
function.::

  static bool filter_function(struct dma_chan *dchan, void *arg)
  {
          struct dma_device *ddev = dchan->device;
          int dev_id = (*((int *)arg) >> 16) & 0xFFFF;
          int chan_id = *((int *)arg) & 0xFFFF;

          return ddev->dev_id == dev_id && dchan->chan_id == chan_id;
  }

  void function(void)
  {
          struct resource *r;
          int dma_dev_id;
          dma_cap_mask_t dma_mask;

          /* ... */

          r = platform_get_resource(pdev, IORESOURCE_DMA, TDC_DMA);
          dma_dev_id = r->start;

          dma_cap_zero(dma_mask);
          dma_cap_set(DMA_SLAVE, dma_mask);
          dma_cap_set(DMA_PRIVATE, dma_mask);
          dchan = dma_request_channel(dma_mask, filter_function,
	                              dma_dev_id);

          /* ... */
  }

You can get the maximum transfer size by calling ``dma_get_max_seg_size()``.::

  dma_get_max_seg_size(dchan->device->dev);

.. warning::
   The GN4124 chip has a 4KiB payload. When doing a ``DMA_DEV_TO_MEM``
   the HDL DMA engine splits transfers in 4KiB chunks, but for
   ``DMA_MEM_TO_DEV`` transfers the split should happen in the
   driver: it does not happen. The DMA engine implementation
   supports ``DMA_MEM_TO_DEV`` manly for testing purposes; to avoid
   complications in the driver the 4KiB split is left to users.
