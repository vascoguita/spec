/*
 * Copyright (C) 2020 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */

#ifndef __LINUX_UAPI_SPEC_H
#define __LINUX_UAPI_SPEC_H
#ifndef __KERNEL__
#include <stdint.h>
#endif

#define SPEC_FMC_SLOTS 1

/* On FPGA components */
#define PCI_VENDOR_ID_CERN      (0x10DC)
#define PCI_DEVICE_ID_SPEC_45T  (0x018D)
#define PCI_DEVICE_ID_SPEC_100T (0x01A2)
#define PCI_DEVICE_ID_SPEC_150T (0x01A3)
#define PCI_VENDOR_ID_GENNUM    (0x1A39)
#define PCI_DEVICE_ID_GN4124    (0x0004)

#define GN4124_GPIO_MAX 16
#define GN4124_GPIO_BOOTSEL0 15
#define GN4124_GPIO_BOOTSEL1 14
#define GN4124_GPIO_SPRI_DIN 13
#define GN4124_GPIO_SPRI_FLASH_CS 12
#define GN4124_GPIO_IRQ0 9
#define GN4124_GPIO_IRQ1 8
#define GN4124_GPIO_SCL 5
#define GN4124_GPIO_SDA 4

#define SPEC_DDR_SIZE (256 * 1024 * 1024)

#define SPEC_META_VENDOR_ID PCI_VENDOR_ID_CERN
#define SPEC_META_DEVICE_ID 0x53504543
#define SPEC_META_BOM_LE 0xFFFE0000
#define SPEC_META_BOM_END_MASK 0xFFFF0000
#define SPEC_META_BOM_VER_MASK 0x0000FFFF
#define SPEC_META_VERSION_MASK 0xFFFF0000
#define SPEC_META_VERSION_2_0 0x02000000

/**
 * struct spec_meta_id Metadata
 */
struct spec_meta_id {
	uint32_t vendor;
	uint32_t device;
	uint32_t version;
	uint32_t bom;
	uint32_t src[4];
	uint32_t cap;
	uint32_t uuid[4];
};

#define SPEC_META_VERSION_MAJ(_v) ((_v >> 24) & 0xFF)
#define SPEC_META_VERSION_MIN(_v) ((_v >> 16) & 0xFF)
#define SPEC_META_VERSION_PATCH(_v) (_v & 0xFFFF)

#endif /* __LINUX_UAPI_SPEC_H */
