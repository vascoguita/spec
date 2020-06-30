"""
SPDX-License-Identifier: GPL-3.0-or-later
SPDX-FileCopyrightText: 2020 CERN
"""
import pytest
import random
import math
from PySPEC import PySPEC

random_repetitions = 0

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

    def test_dma_no_buffer(self, spec):
        """
        The read/write will return immediatelly if asked to perform
        0-length transfer.
        """
        spec.dma_start()
        data = spec.dma_read(0, 0)
        spec.dma_stop()
        assert len(data) == 0

        spec.dma_start()
        count = spec.dma_write(0, b"")
        spec.dma_stop()
        assert count == 0

    @pytest.mark.parametrize("buffer_size",
                             [2**i for i in range(3, 22)])
    def test_dma_read(self, spec, buffer_size):
        """
        We just want to see if the DMA engine reports errors. Test the
        engine with different sizes, but same offset (default:
        0x0). On the engine side we will get several transfers
        (scatterlist) depending on the size.
        """
        spec.dma_start()
        data1 = spec.dma_read(0, buffer_size)
        data2 = spec.dma_read(0, buffer_size)
        spec.dma_stop()

        assert len(data1) == buffer_size
        assert len(data2) == buffer_size
        assert data1 == data2

    @pytest.mark.parametrize("buffer_size",
                             [2**i for i in range(3, 22)])
    def test_dma_write(self, spec, buffer_size):
        """
        We just want to see if the DMA engine reports errors. Test the
        engine with different sizes, but same offset (default:
        0x0). On the engine side we will get several transfers
        (scatterlist) depending on the size.
        """
        spec.dma_start()
        count = spec.dma_write(0, b"\x00" * buffer_size)
        assert count == buffer_size
        spec.dma_stop()

    @pytest.mark.parametrize("ddr_offset",
                             [2**i for i in range(2, int(math.log2(PySPEC.DDR_SIZE)))])
    @pytest.mark.parametrize("unaligned", range(1, PySPEC.DDR_ALIGN))
    def test_dma_unaligned_offset_read(self, spec, ddr_offset, unaligned):
        """
        The DDR access is 4byte aligned.
        """
        spec.dma_start()
        with pytest.raises(OSError) as error:
            spec.dma_read(ddr_offset + unaligned, 16)
        spec.dma_stop()

    @pytest.mark.parametrize("ddr_offset",
                             [2**i for i in range(2, int(math.log2(PySPEC.DDR_SIZE)))])
    @pytest.mark.parametrize("unaligned", range(1, PySPEC.DDR_ALIGN))
    def test_dma_unaligned_offset_write(self, spec, ddr_offset, unaligned):
        """
        The DDR access is 4byte aligned.
        """
        spec.dma_start()
        with pytest.raises(OSError) as error:
            spec.dma_write(ddr_offset + unaligned, b"\x00" * 16)
        spec.dma_stop()

    @pytest.mark.parametrize("ddr_offset",
                             [2**i for i in range(2, int(math.log2(PySPEC.DDR_SIZE)))])
    @pytest.mark.parametrize("unaligned", range(1, PySPEC.DDR_ALIGN))
    def test_dma_unaligned_size_read(self, spec, ddr_offset, unaligned):
        """
        The DDR access is 4byte aligned.
        """
        spec.dma_start()
        with pytest.raises(OSError) as error:
            spec.dma_read(ddr_offset, (16 + unaligned))
        spec.dma_stop()

    @pytest.mark.parametrize("ddr_offset",
                             [2**i for i in range(2, int(math.log2(PySPEC.DDR_SIZE)))])
    @pytest.mark.parametrize("unaligned", range(1, PySPEC.DDR_ALIGN))
    def test_dma_unaligned_size_write(self, spec, ddr_offset, unaligned):
        """
        The DDR access is 4byte aligned.
        """
        spec.dma_start()
        with pytest.raises(OSError) as error:
            spec.dma_write(ddr_offset, b"\x00" * (16 + unaligned))
        spec.dma_stop()

    @pytest.mark.parametrize("split", [2**i for i in range(3, 14)])
    @pytest.mark.parametrize("ddr_offset", [0x0, ])
    @pytest.mark.parametrize("buffer_size", [2**14, ])
    def test_dma_split_read(self, spec, buffer_size, ddr_offset, split, capsys):
        """
        Write and read back buffers using DMA. We test different combinations
        of offset and size. Here we atrificially split the **read** in small
        pieces.

        In the past we had problems with transfers greater than 4KiB. Be sure
        that we can perform transfers (read) of "any" size.
        """
        data = bytes([random.randrange(0, 0xFF, 1) for i in range(buffer_size)])
        spec.dma_start()
        spec.dma_write(ddr_offset, data)
        fail = False
        for offset in range(0, buffer_size, split):
            data_rb = spec.dma_read(ddr_offset + offset, split)
            assert data[offset:min(offset+split, buffer_size)] == data_rb, \
              "offset: {:d}".format(offset)
        spec.dma_stop()

        assert fail == False

    @pytest.mark.parametrize("split", [2**i for i in range(3, 14)])
    @pytest.mark.parametrize("ddr_offset", [0x0, ])
    @pytest.mark.parametrize("buffer_size", [2**14, ])
    def test_dma_split_write(self, spec, buffer_size, ddr_offset, split):
        """
        Write and read back buffers using DMA. We test different combinations
        of offset and size. Here we atrificially split the **write** in small
        pieces.

        In the past we had problems with transfers greater than 4KiB. Be sure
        that we can perform transfers (write) of "any" size.
        """
        data = bytes([random.randrange(0, 0xFF, 1) for i in range(buffer_size)])
        spec.dma_start()
        for offset in range(0, buffer_size, split):
            spec.dma_write(offset, data[offset:min(offset+split, buffer_size)])
        data_rb = spec.dma_read(0x0, buffer_size)
        assert data == data_rb, "offset: {:d}".format(offset)
        spec.dma_stop()


    @pytest.mark.parametrize("split", [2**i for i in range(3, 14)])
    @pytest.mark.parametrize("buffer_size", [2**14, ])
    def test_dma_split(self, spec, buffer_size, split):
        """
        Write and read back buffers using DMA. We test different combinations
        of offset and size. Here we atrificially split transfers in small
        pieces.

        In the past we had problems with transfers greater than 4KiB. Be sure
        that we can perform transfers of "any" size.
        """
        data = bytes([random.randrange(0, 0xFF, 1) for i in range(buffer_size)])
        spec.dma_start()
        for offset in range(0, buffer_size, split):
            spec.dma_write(offset, data[offset:min(offset+split, buffer_size)])
        for offset in range(0, buffer_size, split):
            data_rb = spec.dma_read(offset, split)
            assert data[offset:min(offset+split, buffer_size)] == data_rb
        spec.dma_stop()

    @pytest.mark.parametrize("ddr_offset",
                             [0x0] + \
                             [2**i for i in range(int(math.log2(PySPEC.DDR_ALIGN)), int(math.log2(PySPEC.DDR_SIZE)))] + \
                             [random.randrange(0, PySPEC.DDR_SIZE, PySPEC.DDR_ALIGN) for x in range(random_repetitions)])
    @pytest.mark.parametrize("buffer_size",
                             [2**i for i in range(int(math.log2(PySPEC.DDR_ALIGN)) + 1, 22)] + \
                             [random.randrange(PySPEC.DDR_ALIGN * 2, 4096, PySPEC.DDR_ALIGN) for x in range(random_repetitions)])
    def test_dma(self, spec, ddr_offset, buffer_size):
        """
        Write and read back buffers using DMA. We test different combinations
        of offset and size. Here we try to perform transfers as large as
        possible (short scatterlist)
        """
        if ddr_offset + buffer_size >= PySPEC.DDR_SIZE:
            pytest.skip("DDR Overflow!")

        data = bytes([random.randrange(0, 0xFF, 1) for i in range(buffer_size)])

        spec.dma_start()
        spec.dma_write(ddr_offset, data)
        data_rb = spec.dma_read(ddr_offset, buffer_size)
        spec.dma_stop()

        assert data == data_rb
