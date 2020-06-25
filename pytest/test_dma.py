"""
SPDX-License-Identifier: GPL-3.0-or-later
SPDX-FileCopyrightText: 2020 CERN
"""
import pytest
import random
from PySPEC import PySPEC

class TestDma(object):
    def test_acquisition_release(self, spec):
        """
        Users can open and close the DMA channel
        """
        spec.dma_start()
        spec.dma_stop()

    def test_acquisition_release_contention(self, spec):
        """
        Refuse simultaneous DMA transfers
        """
        spec.dma_start()
        spec_c = PySPEC(spec.pci_id)
        with pytest.raises(OSError) as error:
            spec_c.dma_start()
        spec.dma_stop()

    @pytest.mark.parametrize("dma_size",
                             [random.randrange(0, 1 * 1024 * 1024, 4) for i in range(100)])
    def test_dma_read(self, spec, dma_size):
        """
        Read from SPEC DDR at random offsets. We just want to see if the DMA
        engine reports errors
        """
        spec.dma_start()
        data = spec.dma_read(dma_size)
        assert len(data) == dma_size
        spec.dma_stop()
