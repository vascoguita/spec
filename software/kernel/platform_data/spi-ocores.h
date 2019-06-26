// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2019 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */

#ifndef __SPI_OCORES_PDATA_H__
#define __SPI_OCORES_PDATA_H__

struct spi_ocores_platform_data {
	unsigned int big_endian;
	unsigned int clock_hz;
};

#endif
