..
  SPDX-License-Identifier: CC-BY-SA-4.0+
  SPDX-FileCopyrightText: 2019-2020 CERN

================================
Welcome to SPEC's documentation!
================================

The Simple PCIe FMC Carrier (SPEC) is a 4 lane PCIe card that has an
FPGA and can hold one FMC module and one SFP connector.

Its bridge to the PCIe bus is the Gennum GN4124 chip and its purpose
is to create a bridge between the PCIe bus and the FPGA. With the
exception of the M25P32 FLASH memory, all components are connected to
the FPGA. This implies that an FPGA configuration is necessary to
fully use the card.

The `SPEC project`_ is hosted on the `Open HardWare Repository`_

You can clone the GIT project with the following command::

  git clone https://ohwr.org/project/spec.git

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   hdl-spec-base
   sw-driver
   sw-python

.. _`Open HardWare Repository`: https://ohwr.org/
.. _`SPEC project`: https://ohwr.org/project/spec
