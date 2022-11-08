"""
SPDX-License-Identifier: LGPL-2.1-or-later
SPDX-FileCopyrightText: 2020 CERN
"""

import pytest
from PySPEC import PySPEC


@pytest.fixture(scope="module")
def spec():
    spec_dev = PySPEC(pytest.pci_id)
    yield spec_dev

@pytest.fixture(scope="class")
def dma():
    spec = PySPEC(pytest.pci_id)
    with spec.dma() as spec_dma:
        yield spec_dma

def pytest_addoption(parser):
    parser.addoption("--pci-id",
                     required=True, help="SPEC PCI Identifier")
    parser.addoption("--bitstream",
                     default=None, help="SPEC bitstream to be tested")

def pytest_configure(config):
    pytest.pci_id = config.getoption("--pci-id")
    pytest.cfg_bitstream = config.getoption("--bitstream")

    if pytest.cfg_bitstream is not None:
        spec = PySPEC(pytest.pci_id)
        spec.program_fpga(pytest.cfg_bitstream)
        del spec
