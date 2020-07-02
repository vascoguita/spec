"""
@package docstring
@author: Federico Vaga <federico.vaga@cern.ch>

SPDX-License-Identifier: LGPL-3.0-or-later
SPDX-FileCopyrightText: 2020 CERN  (home.cern)
"""

import os
from contextlib import contextmanager

class PySPEC:
    """
    This class gives access to SPEC features.
    """

    #: SPEC DDR size
    DDR_SIZE = 256 * 1024 * 1024
    #: SPEC DDR access alignment
    DDR_ALIGN = 4

    def __init__(self, pci_id):
        self.pci_id = pci_id
        self.debugfs =  "/sys/kernel/debug/0000:{:s}".format(self.pci_id)
        self.debugfs_fpga = os.path.join(self.debugfs, "spec-0000:{:s}".format(self.pci_id))

    def program_fpga(self, bitstream):
        """
        Program the FPGA with the given bitstream

        :var path: path to bitstream
        :raise OSError: on failure
        """
        with open("/sys/module/firmware_class/parameters/path", "r") as f:
            prev = f.read()
        with open("/sys/module/firmware_class/parameters/path", "w") as f:
            f.write(os.path.dirname(bitstream))
        with open(os.path.join(self.debugfs, "fpga_firmware"), "w") as f:
            f.write(os.path.basename(bitstream))
        with open("/sys/module/firmware_class/parameters/path", "w") as f:
            f.write(os.path.dirname(prev))

    @contextmanager
    def dma(self):
        """
        Create a DMA context from which users can do DMA
        transfers. Within this context the user can use
        PySPECDMA.read() and PySPECDMA.write(). Here an example.

        >>> from PySPEC import PySPEC
        >>> spec = PySPEC("06:00.0")
        >>> with spec.dma() as dma:
        >>>     cnt = dma.write(0, b"\x00" * 16)
        >>>     buffer = dma.read(0, 16)

        Which is equivalent to:

        >>> from PySPEC import PySPEC
        >>> spec = PySPEC("06:00.0")
        >>> spec_dma = PySPEC.PySPECDMA(spec)
        >>> spec_dma.open()
        >>> cnt = spec_dma.write(0, b"\x00" * 16)
        >>> buffer = spec_dma.read(0, 16)
        >>> spec_dma.close()
        """
        spec_dma = self.PySPECDMA(self)
        spec_dma.request()
        try:
            yield spec_dma
        finally:
            spec_dma.release()

    class PySPECDMA:
        """
        This class wraps DMA features in a single object.

        The SPEC has
        only one DMA channel. On request() the user will get exclusive
        access. The user must release() the DMA channel as soon as
        possible to let other users or drivers to access it. For this reason,
        avoid to use this class directly. Instead, use the DMA context
        from the PySPEC class which is less error prone.

        >>> from PySPEC import PySPEC
        >>> spec = PySPEC("06:00.0")
        >>> with spec.dma() as dma:
        >>>     cnt = dma.write(0, b"\x00" * 16)
        >>>     buffer = dma.read(0, 16)
        >>> print(buffer)
        """

        def __init__(self, spec):
            """
            Create a new instance

            :var spec: a valid PySPEC instance
            """
            self.spec = spec

        def request(self):
            """
            Open a DMA file descriptor

            :raise OSError: if the open(2) or the driver fails
            """
            self.dma_file = open(os.path.join(self.spec.debugfs_fpga, "dma"),
                                 "rb+", buffering=0)
        def release(self):
            """
            Close the DMA file descriptor

            :raise OSError: if the close(2) or the driver fails
            """
            if hasattr(self, "dma_file"):
                self.dma_file.close()

        def read(self, offset, size):
            """
            Trigger a *device to memory* DMA transfer

            :var offset: offset within the DDR
            :var size: number of bytes to be transferred
            :return: the data transfered as bytes() array
            :raise OSError: if the read(2) or the driver fails
            """
            self.__seek(offset)
            data = []
            while size - len(data) > 0:
                data += self.dma_file.read(size - len(data))
            return bytes(data)

        def write(self, offset, data):
            """
            Trigger a *memory to device* DMA transfer

            :var offset: offset within the DDR
            :var size: number of bytes to be transferred
            :return: the number of transfered bytes
            :raise OSError: if the write(2) or the driver fails
            """
            self.__seek(offset)
            start = 0
            while len(data) - start > 0:
                start += self.dma_file.write(bytes(data[start:]))
            return start

        def __seek(self, offset):
            """
            Change DDR offset

            :var offset: offset within the DDR
            :raise OSError: if lseek(2) fails or the driver
            """
            self.dma_file.seek(offset)
