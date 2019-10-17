=========
Changelog
=========

[1.4.3] - 2019-10-17
====================
Fixed
-----
- [sw] fix SPEC GPIO get_direction

[1.4.2] - 2019-10-15
====================
Fixed
-----
- [sw] fix SPEC driver dependency with I2C OCores

[1.4.1] - 2019-09-23
====================
Changed
-------
- [sw] do not used devm_* operations (it seems to solve problems)
Removed
-------
- [sw] Removed IRQ line assignment to FCL (not used)
Fixed
-----
- [sw] kcalloc usage
- [sw]  memcpy(), memset() usage
- [sw] checkpatch style fixes

[1.4.0] 2019-09-11
==================
Added
-----
- [hdl] spec-base IP-core to support SPEC based designs
- [sw] Driver for GN4124 FCL using Linux FPGA manager
- [sw] Driver for GN4124 GPIO using Linux GPIOlib
- [sw] Driver for gn412x-core DMA using Linux DMA engine
- [sw] Support for spec-base IP-core
- [sw] Support for FMC

[0.0.0]
=======
Start the development of a new SPEC driver and SPEC HDL support layer
