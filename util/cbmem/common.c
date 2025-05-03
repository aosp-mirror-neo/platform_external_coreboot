/* SPDX-License-Identifier: GPL-2.0-only */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <commonlib/bsd/cbmem_id.h>
#include <commonlib/bsd/helpers.h>
#include <commonlib/coreboot_tables.h>

#include "cbmem_util.h"

void hexdump(const uintptr_t start_address, const uint8_t *buf, const int length)
{
	int i;
	int all_zero = 0;

	for (i = 0; i < length; i += 16) {
		int j;

		all_zero++;
		for (j = 0; j < 16; j++) {
			if (buf[i + j] != 0) {
				all_zero = 0;
				break;
			}
		}

		if (all_zero < 2) {
			printf("%08lx:", start_address + i);
			for (j = 0; j < 16; j++)
				printf(" %02x", buf[i + j]);
			printf("  ");
			for (j = 0; j < 16; j++)
				printf("%c", isprint(buf[i + j]) ? buf[i + j] : '.');
			printf("\n");
		} else if (all_zero == 2) {
			printf("...\n");
		}
	}
}

struct cbmem_id_to_name {
	uint32_t id;
	const char *name;
};
static const struct cbmem_id_to_name cbmem_ids[] = { CBMEM_ID_TO_NAME_TABLE };

#define MAX_STAGEx 10
void cbmem_print_entry(int n, uint32_t id, uint64_t base, uint64_t size)
{
	const char *name;
	char stage_x[20];

	name = NULL;
	for (size_t i = 0; i < ARRAY_SIZE(cbmem_ids); i++) {
		if (cbmem_ids[i].id == id) {
			name = cbmem_ids[i].name;
			break;
		}
		if (id >= CBMEM_ID_STAGEx_META &&
			id < CBMEM_ID_STAGEx_META + MAX_STAGEx) {
			snprintf(stage_x, sizeof(stage_x), "STAGE%d META",
				(id - CBMEM_ID_STAGEx_META));
			name = stage_x;
		}
		if (id >= CBMEM_ID_STAGEx_CACHE &&
			id < CBMEM_ID_STAGEx_CACHE + MAX_STAGEx) {
			snprintf(stage_x, sizeof(stage_x), "STAGE%d $  ",
				(id - CBMEM_ID_STAGEx_CACHE));
			name = stage_x;
		}
	}

	printf("%2d. ", n);
	if (name == NULL)
		name = "(unknown)";
	printf("%-20s  %08x", name, id);
	printf("  %08" PRIx64 " ", base);
	printf(" %08" PRIx64 "\n", size);
}

void cbmem_get_lb_table_entry(uint32_t tag, uint8_t **buf_out, size_t *size_out)
{
	const struct lb_record *lbr_p;
	const uint8_t *lbtable_raw;
	size_t table_size;
	bool tag_found = false;

	if (!cbmem_drv_get_cbmem_entry(CBMEM_ID_CBTABLE, (uint8_t **)&lbtable_raw,
					  &table_size))
		die("coreboot table not found.\n");

	const struct lb_header *lbh = (const struct lb_header *)lbtable_raw;

	for (size_t i = 0; i < lbh->table_bytes; i += lbr_p->size) {
		lbr_p = (const struct lb_record *)(&lbtable_raw[lbh->header_bytes + i]);
		if (lbr_p->tag == tag) {
			tag_found = true;
			break;
		}
	}

	if (!tag_found) {
		free((void *)lbtable_raw);
		die("coreboot table entry %#x not found.\n", tag);
	}

	debug("coreboot table entry %#x found.\n", tag);

	*buf_out = malloc(lbr_p->size);
	if (!*buf_out) {
		free((void *)lbtable_raw);
		die("Unable to allocate memory for coreboot table entry %#x, size: %d\n", tag,
		    lbr_p->size);
	}
	memcpy(*buf_out, lbr_p, lbr_p->size);
	*size_out = lbr_p->size;
	free((void *)lbtable_raw);
}
