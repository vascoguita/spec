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
                             [0, 1, 2, 3] + \
                             [5, 7, 9] + \
                             [2**i for i in range(24)])
    def test_dma_read(self, spec, dma_size):
        """
        We just want to see if the DMA engine reports errors. Test the
        engine with different sizes, but same offset (default:
        0x0). On the engine side we will get several transfers
        (scatterlist) depending on the size.

        Regressions:
        - 0: the driver returns immediatly without starting any DMA transfer
        - 1, 2, 3, 4: performing transfers of these sizes led to a failure
        """
        spec.dma_start()
        data1 = spec.dma_read(0, dma_size)
        assert len(data1) == dma_size
        data2 = spec.dma_read(0, dma_size)
        assert len(data2) == dma_size
        spec.dma_stop()

        assert data1 == data2

    @pytest.mark.parametrize("dma_size",
                             [0, 1, 2, 3] + \
                             [5, 7, 9] + \
                             [2**i for i in range(24)])
    def test_dma_write(self, spec, dma_size):
        """
        We just want to see if the DMA engine reports errors. Test the
        engine with different sizes, but same offset (default:
        0x0). On the engine side we will get several transfers
        (scatterlist) depending on the size.

        Regressions:
        - 0: the driver returns immediatly without starting any DMA transfer
        - 1, 2, 3, 4: performing transfers of these sizes led to a failure
        """
        spec.dma_start()
        spec.dma_write(0, b"\x00" * dma_size)
        spec.dma_stop()


    @pytest.mark.parametrize("dma_offset", [0x0])
    @pytest.mark.parametrize("dma_size",
                             [2**i for i in range(3, 22)])
    def test_dma(self, spec, dma_offset, dma_size):
        """
        Write and read back buffers using DMA.
        """
        data = bytes([random.randrange(0, 0xFF, 1) for i in range(dma_size)])

        spec.dma_start()
        spec.dma_write(dma_offset, data)
        data_rb = spec.dma_read(dma_offset, dma_size)
        spec.dma_stop()

        assert data == data_rb
