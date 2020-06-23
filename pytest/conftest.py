"""
SPDX-License-Identifier: GPL-3.0-or-later
SPDX-FileCopyrightText: 2020 CERN
"""

import pytest
from PySPEC import PySPEC


@pytest.fixture(scope="function")
def spec():
    return PySPEC(pytest.pci_id)

def pytest_addoption(parser):
    parser.addoption("--pci-id",
                     required=True, help="SPEC PCI Identifier")

def pytest_configure(config):
    pytest.pci_id = config.getoption("--pci-id")
