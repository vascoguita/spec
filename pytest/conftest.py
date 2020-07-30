"""
SPDX-License-Identifier: GPL-3.0-or-later
SPDX-FileCopyrightText: 2020 CERN
"""

import pytest
from PySPEC import PySPEC


@pytest.fixture(scope="function")
def spec():
    spec_dev = PySPEC(pytest.pci_id)
    yield spec_dev

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
