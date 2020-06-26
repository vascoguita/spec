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
    spec_dev.dma_stop()

def pytest_addoption(parser):
    parser.addoption("--pci-id",
                     required=True, help="SPEC PCI Identifier")

def pytest_configure(config):
    pytest.pci_id = config.getoption("--pci-id")
