"""
SPDX-License-Identifier: GPL-3.0-or-later
SPDX-FileCopyrightText: 2020 CERN
"""
import pytest
import random
import math
import os
import re
from PySPEC import PySPEC

random_repetitions = 0

class TestDma(object):
    def test_acquisition_release(self, spec):
        """
        Users can open and close the DMA channel
        """
        with spec.dma() as dma:
            pass

    def test_acquisition_release_contention(self, spec):
        """
        Refuse simultaneous DMA transfers
        """
        with spec.dma() as dma:
            spec_c = PySPEC(spec.pci_id)
            with pytest.raises(OSError) as error:
                with spec_c.dma() as dma2:
                    pass

    def test_dma_no_buffer(self, dma):
        """
        The read/write will return immediatelly if asked to perform
        0-length transfer.
        """
        data = dma.read(0, 0)
        assert len(data) == 0
        count = dma.write(0, b"")
        assert count == 0

    @pytest.mark.parametrize("buffer_size",
                             [2**i for i in range(3, 22)])
    def test_dma_read(self, dma, buffer_size):
        """
        We just want to see if the DMA engine reports errors. Test the
        engine with different sizes, but same offset (default:
        0x0). On the engine side we will get several transfers
        (scatterlist) depending on the size.
        """
        data1 = dma.read(0, buffer_size)
        data2 = dma.read(0, buffer_size)
        assert len(data1) == buffer_size
        assert len(data2) == buffer_size
        assert data1 == data2

    @pytest.mark.parametrize("buffer_size",
                             [2**i for i in range(3, 22)])
    def test_dma_write(self, dma, buffer_size):
        """
        We just want to see if the DMA engine reports errors. Test the
        engine with different sizes, but same offset (default:
        0x0). On the engine side we will get several transfers
        (scatterlist) depending on the size.
        """
        count = dma.write(0, b"\x00" * buffer_size)
        assert count == buffer_size

    @pytest.mark.parametrize("ddr_offset",
                             [2**i for i in range(2, int(math.log2(PySPEC.DDR_SIZE)))])
    @pytest.mark.parametrize("unaligned", range(1, PySPEC.DDR_ALIGN))
    def test_dma_unaligned_offset_read(self, dma, ddr_offset, unaligned):
        """
        The DDR access is 4byte aligned.
        """
        with pytest.raises(OSError) as error:
            dma.read(ddr_offset + unaligned, 16)

    @pytest.mark.parametrize("ddr_offset",
                             [2**i for i in range(2, int(math.log2(PySPEC.DDR_SIZE)))])
    @pytest.mark.parametrize("unaligned", range(1, PySPEC.DDR_ALIGN))
    def test_dma_unaligned_offset_write(self, dma, ddr_offset, unaligned):
        """
        The DDR access is 4byte aligned.
        """
        with pytest.raises(OSError) as error:
            dma.write(ddr_offset + unaligned, b"\x00" * 16)

    @pytest.mark.parametrize("ddr_offset",
                             [2**i for i in range(2, int(math.log2(PySPEC.DDR_SIZE)))])
    @pytest.mark.parametrize("unaligned", range(1, PySPEC.DDR_ALIGN))
    def test_dma_unaligned_size_read(self, dma, ddr_offset, unaligned):
        """
        The DDR access is 4byte aligned.
        """
        with pytest.raises(OSError) as error:
            dma.read(ddr_offset, (16 + unaligned))

    @pytest.mark.parametrize("ddr_offset",
                             [2**i for i in range(2, int(math.log2(PySPEC.DDR_SIZE)))])
    @pytest.mark.parametrize("unaligned", range(1, PySPEC.DDR_ALIGN))
    def test_dma_unaligned_size_write(self, dma, ddr_offset, unaligned):
        """
        The DDR access is 4byte aligned.
        """
        with pytest.raises(OSError) as error:
            dma.write(ddr_offset, b"\x00" * (16 + unaligned))

    @pytest.mark.parametrize("split", [2**i for i in range(3, 14)])
    @pytest.mark.parametrize("ddr_offset", [0x0, ])
    @pytest.mark.parametrize("buffer_size", [2**14, ])
    def test_dma_split_read(self, dma, buffer_size, ddr_offset, split, capsys):
        """
        Write and read back buffers using DMA. We test different combinations
        of offset and size. Here we atrificially split the **read** in small
        pieces.

        In the past we had problems with transfers greater than 4KiB. Be sure
        that we can perform transfers (read) of "any" size.
        """
        data = bytes([random.randrange(0, 0xFF, 1) for i in range(buffer_size)])
        dma.write(ddr_offset, data)
        data_rb = b""
        for offset in range(0, buffer_size, split):
            data_rb += dma.read(ddr_offset + offset, split)
        assert data == data_rb

    @pytest.mark.parametrize("split", [2**i for i in range(3, 14)])
    @pytest.mark.parametrize("ddr_offset", [0x0, ])
    @pytest.mark.parametrize("buffer_size", [2**14, ])
    def test_dma_split_write(self, dma, buffer_size, ddr_offset, split):
        """
        Write and read back buffers using DMA. We test different combinations
        of offset and size. Here we atrificially split the **write** in small
        pieces.

        In the past we had problems with transfers greater than 4KiB. Be sure
        that we can perform transfers (write) of "any" size.
        """
        data = bytes([random.randrange(0, 0xFF, 1) for i in range(buffer_size)])
        for offset in range(0, buffer_size, split):
            dma.write(offset, data[offset:min(offset+split, buffer_size)])
        data_rb = dma.read(0x0, buffer_size)
        assert data == data_rb

    @pytest.mark.parametrize("split", [2**i for i in range(3, 14)])
    @pytest.mark.parametrize("buffer_size", [2**14, ])
    def test_dma_split(self, dma, buffer_size, split):
        """
        Write and read back buffers using DMA. We test different combinations
        of offset and size. Here we atrificially split transfers in small
        pieces.

        In the past we had problems with transfers greater than 4KiB. Be sure
        that we can perform transfers of "any" size.
        """
        data = bytes([random.randrange(0, 0xFF, 1) for i in range(buffer_size)])
        for offset in range(0, buffer_size, split):
            dma.write(offset, data[offset:min(offset+split, buffer_size)])
        for offset in range(0, buffer_size, split):
            data_rb = dma.read(offset, split)
            assert data[offset:min(offset+split, buffer_size)] == data_rb

    @pytest.mark.parametrize("segment_size", [2**i for i in range(3, 20)])
    def test_dma_scatterlist_read(self, dma, segment_size):
        """
        Enforce a scatterlist on known size to read 1MiB.
        """
        buffer_size = 2**20
        data = bytes([random.randrange(0, 0xFF, 1) for i in range(buffer_size)])
        dma.write(0, data)
        assert data == dma.read(0, len(data), segment_size)

    @pytest.mark.parametrize("segment_size", [2**i for i in range(3, 12)])
    def test_dma_scatterlist_write(self, dma, segment_size):
        """
        Enforce a scatterlist on known size to write 1MiB.
        Remember: on write the scatterlist is enforced by the driver
        on buffers bigger than 4KiB
        """
        buffer_size = 2**20
        data = bytes([random.randrange(0, 0xFF, 1) for i in range(buffer_size)])
        dma.write(0, data, segment_size)
        assert data == dma.read(0, len(data))

    @pytest.mark.parametrize("ddr_offset",
                             [0x0] + \
                             [2**i for i in range(int(math.log2(PySPEC.DDR_ALIGN)), int(math.log2(PySPEC.DDR_SIZE)))] + \
                             [random.randrange(0, PySPEC.DDR_SIZE, PySPEC.DDR_ALIGN) for x in range(random_repetitions)])
    @pytest.mark.parametrize("buffer_size",
                             [2**i for i in range(int(math.log2(PySPEC.DDR_ALIGN)) + 1, 22)] + \
                             [random.randrange(PySPEC.DDR_ALIGN * 2, 4096, PySPEC.DDR_ALIGN) for x in range(random_repetitions)])
    def test_dma(self, dma, ddr_offset, buffer_size):
        """
        Write and read back buffers using DMA. We test different combinations
        of offset and size. Here we try to perform transfers as large as
        possible.
        """
        if ddr_offset + buffer_size >= PySPEC.DDR_SIZE:
            pytest.skip("DDR Overflow!")

        data = bytes([random.randrange(0, 0xFF, 1) for i in range(buffer_size)])
        dma.write(ddr_offset, data)
        data_rb = dma.read(ddr_offset, buffer_size)
        assert data == data_rb

    def test_dma_reg_zero(self, dma):
        """
        Regression test.
        It happend that after 256Bytes the received data is just
        0x00. Other times it happend at different sizes but always
        among the first power of two values
        """
        data = bytes([random.randrange(0, 0xFF, 1) for i in range(1024)])
        dma.write(0, data)
        for i in range(1000000):
            assert data == dma.read(0, len(data))

    @pytest.mark.skipif(pytest.cfg_bitstream is None,
                        reason="We need a bitstream to reflash")
    def test_dma_reg_word(self, dma):
        """
        Regression test.
        It happend that a 4Bytes transfer was triggering a DMA error
        and in some cases it was irremediably corrupting the gennum chip.
        This is most likely to happen once after programming the FPGA, and
        it may take a long time before it happens again.
        """
        spec.program_fpga(pytest.cfg_bitstream)
        data = b"0123456789abcdefghilmnopqrstuvwx"
        dma.write(0, data)
        for i in range(10000000):
            try:
                assert data[0:4] == dma.read(0, 4)
            except OSError as error:
                assert False, "Failed after {:d} transfers".format(i)

class TestDmaPerformance(object):
    @pytest.mark.parametrize("dma_alloc_size",
                             [2**20 * x for x in range(1, 5)])
    def test_dma_throughput_read(self, spec, dma_alloc_size):
        """
        It roughly measure read throughput by using the kernel
        tracer. It does a simple avarage of 100 acquisitions.
        A safe expectation today is 230MBps
        """
        tracing_path = "/sys/kernel/debug/tracing"
        with open(os.path.join(tracing_path, "current_tracer"), "w") as f:
            f.write("function")
        with open(os.path.join(tracing_path, "set_ftrace_filter"), "w") as f:
            f.write("gn412x_dma_irq_handler\ngn412x_dma_start_task")
        with open(os.path.join(tracing_path, "trace"), "w") as f:
            f.write("")

        diff = []
        for i in range(100):
            with open(os.path.join(tracing_path, "trace"), "w") as f:
                f.write("")
            with spec.dma(dma_alloc_size) as dma:
                data = dma.read(0, dma_alloc_size)
                assert len(data) == dma_alloc_size
            with open(os.path.join(tracing_path, "trace"), "r") as f:
                trace = f.read()

            start = re.search(r"([0-9]+\.[0-9]{6}): gn412x_dma_start_task", trace, re.MULTILINE)
            assert start is not None, trace
            assert len(start.groups()) == 1
            end = re.search(r"([0-9]+\.[0-9]{6}): gn412x_dma_irq_handler", trace, re.MULTILINE)
            assert end is not None, trace
            assert len(end.groups()) == 1
            diff.append(float(end.group(1)) - float(start.group(1)))
            assert diff[-1] > 0

        throughput_m  = int((dma_alloc_size / (round(math.fsum(diff) / 100, 6))) / 1024 / 1024)
        assert throughput_m > 230, \
          "Expected mora than {:d} MBps, got {:d} MBps".format(500,
                                                               throughput_m)
