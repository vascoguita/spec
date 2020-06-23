"""
@package docstring
@author: Federico Vaga <federico.vaga@cern.ch>

SPDX-License-Identifier: LGPL-3.0-or-later
SPDX-FileCopyrightText: 2020 CERN  (home.cern)
"""
import os

class PySPEC:
    def __init__(self, pci_id):
        self.pci_id = pci_id
        self.debugfs =  "/sys/kernel/debug/0000:{:s}".format(self.pci_id)
        self.debugfs_fpga = os.path.join(self.debugfs, "spec-0000:{:s}".format(self.pci_id))
        self.dma = os.path.join(self.debugfs_fpga, "dma")

    def dma_start(self):
        self.dma_file = open(self.dma, "r")

    def dma_stop(self):
        if hasattr(self, "dma_file"):
            self.dma_file.close()
