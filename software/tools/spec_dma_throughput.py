#!/usr/bin/python3

import os
import re
import argparse
import math
from matplotlib import pyplot
from PySPEC import PySPEC

def dma_time_get(trace):
    start = re.search(r"([0-9]+\.[0-9]{6}): gn412x_dma_start_task", trace, re.MULTILINE)
    assert start is not None, trace
    assert len(start.groups()) == 1
    end = re.search(r"([0-9]+\.[0-9]{6}): gn412x_dma_irq_handler", trace, re.MULTILINE)
    assert end is not None, trace
    assert len(end.groups()) == 1
    return round(float(end.group(1)) - float(start.group(1)), 6)

def main():
    parser = argparse.ArgumentParser(description='DMA Throughput')
    parser.add_argument('--pci-id', dest='pciid', required=True,
                        help='SPEC PCI ID to use')
    parser.add_argument('--min', default=4 * 1024, type=int,
                        help='Minimum transfer size in Bytes (default: 4096 Bytes). It is rounded to the lower power of 2.')
    parser.add_argument('--max', default=4 * 1024 * 1024, type=int,
                        help='Maximum transfer size in Bytes (default: 4194304 Bytes). It is rounded to the lower power of 2.')
    args = parser.parse_args()

    tracing_path = "/sys/kernel/debug/tracing"
    with open(os.path.join(tracing_path, "current_tracer"), "w") as f:
        f.write("function")
    with open(os.path.join(tracing_path, "set_ftrace_filter"), "w") as f:
        f.write("gn412x_dma_irq_handler\ngn412x_dma_start_task")
    with open(os.path.join(tracing_path, "trace"), "w") as f:
        f.write("")

    spec = PySPEC(args.pciid)
    throughput = []
    sizes = [2**x for x in range(int(math.log2(args.min)), int(math.log2(args.max)) + 1)]
    for size in sizes:
        with open(os.path.join(tracing_path, "trace"), "w") as f:
            f.write("")
        with spec.dma(size) as dma:
            dma.read(0, size)
        with open(os.path.join(tracing_path, "trace"), "r") as f:
            throughput.append((float(size) / 1024 / 1024) / dma_time_get(f.read()))
        print("{:d} Bytes -> {:f} MBps".format(size, throughput[-1]))

    pyplot.title("DMA throughput at different block sizes")
    pyplot.xlabel("DMA Size in Bytes")
    pyplot.ylabel("Throughput in MBps")
    pyplot.plot(sizes,throughput)
    pyplot.show()

if __name__ == "__main__":
    main()
