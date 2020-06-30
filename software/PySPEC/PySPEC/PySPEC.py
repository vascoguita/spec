"""
@package docstring
@author: Federico Vaga <federico.vaga@cern.ch>

SPDX-License-Identifier: LGPL-3.0-or-later
SPDX-FileCopyrightText: 2020 CERN  (home.cern)
"""
import os
from contextlib import contextmanager

class PySPEC:
    DDR_SIZE = 256 * 1024 * 1024
    DDR_ALIGN = 4

    class PySPECDMA:
        def __init__(self, spec):
            self.dma_file = open(os.path.join(spec.debugfs_fpga, "dma"),
                                 "rb+", buffering=0)

        def close(self):
            self.dma_file.close()

        def read(self, offset, size):
            """
            Trigger a *device to memory* DMA transfer
            """
            self.__seek(offset)
            data = []
            while size - len(data) > 0:
                data += self.dma_file.read(size - len(data))
            return bytes(data)

        def write(self, offset, data):
            """
            Trigger a *device to memory* DMA transfer
            """
            self.__seek(offset)
            start = 0
            while len(data) - start > 0:
                start += self.dma_file.write(bytes(data[start:]))
            return start

        def __seek(self, offset):
            self.dma_file.seek(offset)

    def __init__(self, pci_id):
        self.pci_id = pci_id
        self.debugfs =  "/sys/kernel/debug/0000:{:s}".format(self.pci_id)
        self.debugfs_fpga = os.path.join(self.debugfs, "spec-0000:{:s}".format(self.pci_id))

    @contextmanager
    def dma(self):
        spec_dma = self.PySPECDMA(self)
        try:
            yield spec_dma
        finally:
            spec_dma.close()
