# SPDX-License-Identifier: GPL-3.0-or-later
#
# SPDX-FileCopyrightText: 2022 CERN

# Path to the kernel sources
%{?!with_linux: %global with_linux /usr/src/kernels/`uname -r`}

%global kver %(basename %{with_linux})

#
# General Pacakge Information
#
Summary: Simple PCIe Carrier (SPEC)
Name: spec-fmc-carrier
Version: %{?_version}
License: GPL-3.0-or-later
Release: 1%{?dist}
URL: https://www.ohwr.org/projects/spec

ExclusiveArch: x86_64

BuildRequires: make
BuildRequires: gcc
BuildRequires: git
#BuildRequires: cheby

#
# Meta Pacakge
#
Requires: %{name}-kmod
Requires: %{name}-tools
Requires: %{name}-firmware
Requires: python3-%{name}

Source0: %{name}-%{version}.tar.gz
Source1: CHANGELOG

%description
This package is a meta package installing the SPEC driver and its reference firmware

#
# Preparation
#
%prep
%autosetup -n %{name}-%{version}


#
# Build
#
%build
export VERSION=%{version}
export GIT_VERSION=%{?_git_version}
export LINUX=%{with_linux}

%make_build -C software

#
# Install
#
%install
export VERSION=%{version}
export GIT_VERSION=%{?_git_version}
export DESTDIR=%{buildroot}/%{_exec_prefix}

%make_build -C software/include install
%make_build -C software/tools install
%make_build -C software/PySPEC install
# Install manually to avoid running depmod at this stage
%{__install} -d %{buildroot}/lib/modules/%{kver}/extra/cern/%{name}
%{__install} -t %{buildroot}/lib/modules/%{kver}/extra/cern/%{name} software/kernel/*.ko
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;


#
# Clean
#
%clean
%{__rm} -rf %{buildroot}


#
# Changelog
#
%changelog
%include %{SOURCE1}

#
# Kmod
#
%package kmod
Summary: Simple PCIe Carrier (SPEC) Kmod

%description kmod
The SPEC kmod

%files kmod
%license LICENSES/GPL-3.0-or-later.txt
/lib/modules/%{kver}/extra/cern/%{name}/*.ko


#
# Python Class
#
%package -n python3-%{name}
Summary: Simple PCIe Carrier (SPEC) Python Class
Requires: Python3
BuildRequires: python3-devel

%description -n python3-%{name}
The SPEC python class

%files -n python3-%{name}
%license LICENSES/GPL-3.0-or-later.txt
%pycached %{python3_sitelib}/%{name}/

#
# Tools
#
%package tools
Summary: Simple PCIe Carrier (SPEC) Tools

%description tools
The SPEC tools

%files tools
%license LICENSES/GPL-3.0-or-later.txt
/usr/bin/spec-firmware-version


#
# Devel
#
%package devel
Summary: Simple PCIe Carrier (SPEC) Devel

%description devel
The SPEC development header files

%files devel
%license LICENSES/GPL-3.0-or-later.txt
/usr/include/linux/spec.h
/usr/include/linux/spec-core-fpga.h


#
# Firmware
#
%package firmware
Summary: Simple PCIe Carrier (SPEC) Firmware

%description firmware
The SPEC reference firmware supporing basic features

%files firmware
%license LICENSES/CERN-OHL-W-2.0+.txt
#/lib/firmware/spec-fmc-carrier-golden.bin

