/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2020 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <libgen.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <linux/spec.h>


static const char git_version[] = "git version: " GIT_VERSION;
static char *name;

static void help(void)
{
	fprintf(stderr, "%s [options]\n"
		"\tPrint the firmware version\n"
		"\t-p <PCIID>\n"
		"\t-V  print version\n"
		"\t-h  print help\n",
		name);
}

static void print_version(void)
{
	printf("%s version %s\n", name, git_version);
}

static void cleanup(void)
{
	if (name)
		free(name);
}

#define PCIID_STR_LEN 16

int main(int argc, char *argv[])
{
	struct spec_meta_id *rom;
	int err;
	int fd;
	char path[128];
	char opt;
	char pciid_str[PCIID_STR_LEN] = "\0";

        name = strndup(basename(argv[0]), 64);
	if (!name)
		exit(EXIT_FAILURE);
	err = atexit(cleanup);
	if (err)
		exit(EXIT_FAILURE);

        while ((opt = getopt(argc, argv, "h?Vvp:")) != -1) {
		switch (opt) {
		case 'h':
		case '?':
			help();
			exit(EXIT_SUCCESS);
		case 'V':
			print_version();
			exit(EXIT_SUCCESS);
		case 'p':
			strncpy(pciid_str, optarg, PCIID_STR_LEN);
			break;
		}
	}
	if (strlen(pciid_str) == 0) {
		fputs("PCI ID is mandatory\n", stderr);
		help();
		exit(EXIT_FAILURE);
	}
	snprintf(path, 128, "/sys/bus/pci/devices/%s/resource0", pciid_str);
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Can't open \"%s\": %s\n",
			path, strerror(errno));
		exit(EXIT_FAILURE);
	}

	rom = mmap(NULL, sizeof(*rom), PROT_READ, MAP_SHARED, fd, 0);
	if ((long)rom == -1) {
		fputs("Failed while reading SPEC FPGA ROM\n", stderr);
	} else {
		fprintf(stdout, "vendor       : 0x%08x\n", rom->vendor);
		fprintf(stdout, "device       : 0x%08x\n", rom->device);
		fprintf(stdout, "version      : %u.%u.%u\n",
			SPEC_META_VERSION_MAJ(rom->version),
			SPEC_META_VERSION_MIN(rom->version),
			SPEC_META_VERSION_PATCH(rom->version));
		if ((rom->bom & SPEC_META_BOM_END_MASK) == SPEC_META_BOM_LE)
			fputs("byte-order   : little-endian\n", stdout);
		else
			fputs("byte-order   : wrong or unknown\n", stdout);
		fprintf(stdout, "sources      : %08x%08x%08x%08x\n",
			rom->src[0], rom->src[1], rom->src[2], rom->src[3]);
		fprintf(stdout, "capabilities : 0x%08x\n", rom->vendor);
		fprintf(stdout, "UUID         : %08x%08x%08x%08x\n",
			rom->uuid[0], rom->uuid[1],
			rom->uuid[2], rom->uuid[3]);
	}
	munmap(rom, sizeof(*rom));
	close(fd);

	exit(err ? EXIT_FAILURE : EXIT_SUCCESS);

 }
