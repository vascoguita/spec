"""
@package docstring
@author: Federico Vaga <federico.vaga@cern.ch>

SPDX-License-Identifier: LGPL-3.0-or-later
SPDX-FileCopyrightText: 2020 CERN  (home.cern)
"""
import os

class PySPEC:
    DDR_SIZE = 256 * 1024 * 1024
    DDR_ALIGN = 4

    def __init__(self, pci_id):
        self.pci_id = pci_id
        self.debugfs =  "/sys/kernel/debug/0000:{:s}".format(self.pci_id)
        self.debugfs_fpga = os.path.join(self.debugfs, "spec-0000:{:s}".format(self.pci_id))
        self.dma = os.path.join(self.debugfs_fpga, "dma")

    def __del__(self):
        self.dma_stop()

    def dma_start(self):
        self.dma_file = open(self.dma, "rb+", buffering=0)

    def dma_stop(self):
        if hasattr(self, "dma_file"):
            self.dma_file.close()
            del self.dma_file

    def dma_read(self, offset, size):
        """
        Trigger a *device to memory* DMA transfer
        """
        self.__dma_seek(offset)
        data = []
        while size - len(data) > 0:
            data += self.dma_file.read(size - len(data))
        return bytes(data)

    def dma_write(self, offset, data):
        """
        Trigger a *device to memory* DMA transfer
        """
        self.__dma_seek(offset)
        start = 0
        while len(data) - start > 0:
            start += self.dma_file.write(bytes(data[start:]))
        return start

    def __dma_seek(self, offset):
       self.dma_file.seek(offset)
