/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2020 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include <libgen.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <linux/spec.h>
#include <spec-core-fpga.h>


static const char git_version[] = "git version: " GIT_VERSION;
static char *name;
static bool singleline = false;
static unsigned int verbose;

static void help(void)
{
	fprintf(stderr, "%s [options]\n"
		"\tPrint the firmware version\n"
		"\t-p <PCIID>\n"
		"\t-b  print spec-base\n"
		"\t-a  print spec-application\n"
		"\t-1  print on a single line\n"
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

static const char *bom_to_str(uint32_t bom)
{
	if ((bom & SPEC_META_BOM_END_MASK) == SPEC_META_BOM_LE)
		return "little-endian";
	else
		return "wrong";
}

static void print_meta_id_one(struct spec_meta_id *rom)
{
	fprintf(stdout, "0x%08x,0x%08x,%u.%u.%u",
		rom->vendor,
		rom->device,
		SPEC_META_VERSION_MAJ(rom->version),
		SPEC_META_VERSION_MIN(rom->version),
		SPEC_META_VERSION_PATCH(rom->version));
	if (verbose > 1) {
		fprintf(stdout, ",%08x%08x%08x%08x,%s,%08x%08x%08x%08x",
			rom->src[0], rom->src[1], rom->src[2], rom->src[3],
			bom_to_str(rom->bom),
			rom->uuid[0], rom->uuid[1], rom->uuid[2], rom->uuid[3]);
	}
	fputc('\n', stdout);
}

static void print_meta_id(struct spec_meta_id *rom)
{
	fputc('\n', stdout);
	fprintf(stdout, "  vendor       : 0x%08x\n", rom->vendor);
	fprintf(stdout, "  device       : 0x%08x\n", rom->device);
	fprintf(stdout, "  version      : %u.%u.%u\n",
		SPEC_META_VERSION_MAJ(rom->version),
		SPEC_META_VERSION_MIN(rom->version),
		SPEC_META_VERSION_PATCH(rom->version));
	if (verbose > 1) {
		fprintf(stdout, "  byte-order   : %s\n", bom_to_str(rom->bom));
		fprintf(stdout, "  sources      : %08x%08x%08x%08x\n",
			rom->src[0], rom->src[1], rom->src[2], rom->src[3]);
		fprintf(stdout, "  capabilities : 0x%08x\n", rom->cap);
		fprintf(stdout, "  UUID         : %08x%08x%08x%08x\n",
			rom->uuid[0], rom->uuid[1],
			rom->uuid[2], rom->uuid[3]);
	}
}

static int print_base_meta_id(int fd)
{
	struct spec_meta_id *rom;

	rom = mmap(NULL, sizeof(*rom), PROT_READ, MAP_SHARED, fd,
		   SPEC_BASE_REGS_METADATA);
	if ((long)rom == -1) {
		fputs("Failed while reading SPEC-BASE FPGA ROM\n", stderr);
		return -1;
	}
	fputs("spec-base: ", stdout);
	if (singleline)
		print_meta_id_one(rom);
	else
		print_meta_id(rom);
	munmap(rom, sizeof(*rom));

        return 0;
}

static off_t app_meta_id_offset(int fd)
{
	off_t offset;
	void *regs;

	regs = mmap(NULL, 0x100, PROT_READ, MAP_SHARED, fd, 0);
	if ((long)regs == -1)
		return -1;
	offset = *((uint32_t *)((char *)regs + SPEC_BASE_REGS_CSR_APP_OFFSET));
	munmap(regs, 0x100);

	return offset;
}

static int print_app_meta_id(int fd)
{
	struct spec_meta_id *rom;
	off_t offset;

	offset = app_meta_id_offset(fd);
	if (offset < 0) {
		fputs("Can't get spec-app offset\n", stderr);
		return -1;
	}

	if (offset == 0) {
		fputs("spec-application:\n  None\n", stderr);
		return 0;
	}
	rom = mmap(NULL, sizeof(*rom), PROT_READ, MAP_SHARED, fd, offset);
	if ((long)rom == -1) {
		fputs("Failed while reading SPEC-APP FPGA ROM\n", stderr);
		return -1;
	}
	fputs("spec-application: ", stdout);
	if (singleline)
		print_meta_id_one(rom);
	else
		print_meta_id(rom);
	munmap(rom, sizeof(*rom));

        return 0;
}

#define PCIID_STR_LEN 16
int main(int argc, char *argv[])
{
	bool base = false, app = false;
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

        while ((opt = getopt(argc, argv, "h?Vvp:ba1")) != -1) {
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
		case 'a':
			app = true;
			break;
		case 'b':
			base = true;
			break;
		case 'v':
			verbose++;
			break;
		case '1':
			singleline = true;
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
	if (base)
		err = print_base_meta_id(fd);
	if (app)
		err = print_app_meta_id(fd);
	close(fd);

	exit(err ? EXIT_FAILURE : EXIT_SUCCESS);

 }
