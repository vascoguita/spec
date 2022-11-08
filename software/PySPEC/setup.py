#!/usr/bin/env python

"""
SPDX-License-Identifier: LGPL-2.1-or-later
SPDX-FileCopyrightText: 2020 CERN
"""

from setuptools import setup

setup(name='PySPEC',
      description='Python Module to handle SPEC cards',
      author='Federico Vaga',
      author_email='federico.vaga@cern.ch',
      maintainer="Federico Vaga",
      maintainer_email="federico.vaga@cern.ch",
      url='https://www.ohwr.org/project/spec',
      packages=['PySPEC'],
      license='LGPL-2.1-or-later',
      use_scm_version={
          "write_to":  "PySPEC/_version.py",
          "version_scheme":  "no-guess-dev",
          "local_scheme": "dirty-tag",
          "relative_to": "../../README.rst",

      },
      setup_requires=['setuptools_scm'],
     )
