// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2019 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */

#ifndef __GN412X_GPIO_H__
#define __GN412X_GPIO_H__

struct spi_gcores_platform_data {
	unsigned int big_endian;
	unsigned int num_chipselect;
	unsigned int clock_hz;
};

#endif
