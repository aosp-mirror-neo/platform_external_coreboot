/* SPDX-License-Identifier: GPL-2.0-only */

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(__linux__) || defined(__ANDROID__)
#include <linux/fs.h>
#include <sys/ioctl.h>
#endif

#include <commonlib/bsd/cbmem_id.h>
#include <commonlib/bsd/ipchksum.h>
#include <commonlib/coreboot_tables.h>
#include <commonlib/helpers.h>
#include <commonlib/timestamp_serialized.h>
#include <commonlib/tpm_log_serialized.h>

#include "cbmem_util.h"

#define CBMEM_SYSFS_BASE_DIR               "/sys/bus/coreboot/devices/"
#define CBMEM_SYSFS_ENTRY_DIR_NAME_PREFIX  "cbmem-"
#define CBMEM_SYSFS_ENTRY_DIR_NAME_FMT  CBMEM_SYSFS_ENTRY_DIR_NAME_PREFIX "%08" PRIx32
#define CBMEM_SYSFS_ENTRY_DIR_PATH_FMT     CBMEM_SYSFS_BASE_DIR CBMEM_SYSFS_ENTRY_DIR_NAME_FMT
#define CBMEM_SYSFS_ENTRY_ADDRESS_PATH_FMT CBMEM_SYSFS_ENTRY_DIR_PATH_FMT "/address"
#define CBMEM_SYSFS_ENTRY_MEM_PATH_FMT     CBMEM_SYSFS_ENTRY_DIR_PATH_FMT "/mem"
#define CBMEM_SYSFS_ENTRY_SIZE_PATH_FMT    CBMEM_SYSFS_ENTRY_DIR_PATH_FMT "/size"

enum cbmem_sysfs_path_type {
	CBMEM_SYSFS_PATH_BASE,
	CBMEM_SYSFS_PATH_ADDRESS,
	CBMEM_SYSFS_PATH_MEM,
	CBMEM_SYSFS_PATH_SIZE,
};

/**
 * Create new path for CBMEM entry of a requested type. free() path_out after use.
 *
 * @param id CBMEM_ID_* value.
 * @param t path type.
 * @param path_out output pointer to created path.
 *
 * @returns true on success, false otherwise.
 */
static bool new_sysfs_path(uint32_t id, enum cbmem_sysfs_path_type t, char **path_out)
{
	switch (t) {
	case CBMEM_SYSFS_PATH_BASE:
		return asprintf(path_out, CBMEM_SYSFS_ENTRY_DIR_PATH_FMT, id) > 0;
	case CBMEM_SYSFS_PATH_ADDRESS:
		return asprintf(path_out, CBMEM_SYSFS_ENTRY_ADDRESS_PATH_FMT, id) > 0;
	case CBMEM_SYSFS_PATH_MEM:
		return asprintf(path_out, CBMEM_SYSFS_ENTRY_MEM_PATH_FMT, id) > 0;
	case CBMEM_SYSFS_PATH_SIZE:
		return asprintf(path_out, CBMEM_SYSFS_ENTRY_SIZE_PATH_FMT, id) > 0;
	}
	return false;
}

/* Check if non-directory sysfs path is correct. */
static bool check_path(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f) {
		debug("Unable to open path %s for reading. Error: %s\n", path, strerror(errno));
		return false;
	}

	uint8_t test_byte = 0;
	if (fread(&test_byte, 1, 1, f) != 1) {
		debug("Unable to read data fomr %s. Error: %s\n", path, strerror(errno));
		return false;
	}

	fclose(f);
	return true;
}

bool cbmem_sysfs_init(void)
{
	char *cbtable_address_path = NULL;
	char *cbtable_mem_path = NULL;
	char *cbtable_size_path = NULL;

	if (!new_sysfs_path(CBMEM_ID_CBTABLE, CBMEM_SYSFS_PATH_ADDRESS,
			    &cbtable_address_path) ||
	    !new_sysfs_path(CBMEM_ID_CBTABLE, CBMEM_SYSFS_PATH_MEM, &cbtable_mem_path) ||
	    !new_sysfs_path(CBMEM_ID_CBTABLE, CBMEM_SYSFS_PATH_SIZE, &cbtable_size_path)) {
		free(cbtable_address_path);
		free(cbtable_mem_path);
		free(cbtable_size_path);
		die("Unable to create sysfs path strings for coreboot table.\n");
	}

	/* Quickly check access to the coreboot table CBMEM entry.
	   It is mandatory and if it exists, then CBMEM in sysfs should work fine. */
	bool ret = true;
	if (!check_path(cbtable_address_path) || !check_path(cbtable_mem_path) ||
	    !check_path(cbtable_size_path)) {
		debug("Incorrect paths, parmissions or attributes:\n%s\n%s\n%s\n",
		      cbtable_address_path, cbtable_mem_path, cbtable_size_path);
		ret = false;
	}

	free(cbtable_address_path);
	free(cbtable_mem_path);
	free(cbtable_size_path);

	return ret;
}

bool cbmem_sysfs_probe_cbmem_entry(uint32_t id, uint64_t *addr_out, size_t *size_out)
{
	bool ret = false;
	char *address_path = NULL;
	char *size_path = NULL;

	if (!new_sysfs_path(id, CBMEM_SYSFS_PATH_ADDRESS, &address_path) ||
	    !new_sysfs_path(id, CBMEM_SYSFS_PATH_SIZE, &size_path))
		die("Unable to create address and size file paths for CBMEM entry id: %#x\n",
		    id);

	FILE *address_file = fopen(address_path, "rb");
	if (!address_file) {
		debug("Unable to access CBMEM entry id: %#x address file at %s. Error: %s\n",
		      id, address_path, strerror(errno));
		goto end;
	}

	if (fscanf(address_file, "%" PRIx64, addr_out) != 1) {
		debug("Read from %s failed.\n", address_path);
		fclose(address_file);
		goto end;
	}
	fclose(address_file);

	FILE *size_file = fopen(size_path, "rb");
	if (!size_file) {
		debug("Unable to access CBMEM entry id: %#x size file at %s. Error: %s\n", id,
		      size_path, strerror(errno));
		goto end;
	}

	if (fscanf(size_file, "%zx", size_out) != 1) {
		debug("Read from %s failed.\n", size_path);
		fclose(size_file);
		*addr_out = 0;
		goto end;
	}

	ret = true;
end:
	free(address_path);
	free(size_path);
	return ret;
}

static void fetch_cbmem_entry(const uint32_t id, const size_t size, uint8_t **buf_out)
{
	char *mem_path = NULL;

	if (!new_sysfs_path(id, CBMEM_SYSFS_PATH_MEM, &mem_path))
		die("Unable to create path to memory mapped to the CBMEM entry id: %#x. Error: %s\n",
		    id, strerror(errno));

	FILE *mem_file = fopen(mem_path, "rb");
	if (!mem_file)
		die("Unable to open mem file for CBMEM entry id: %#x at %s. Error: %s\n", id,
		    mem_path, strerror(errno));

	*buf_out = malloc(size);
	if (!buf_out)
		die("Unable to allocate memory for CBMEM entry id: %#x of size: %zuB.\n", id,
		    size);

	if (fread(*buf_out, 1, size, mem_file) != size)
		die("Unable to correctly read memory of CBMEM entry id: %#x at %s. Error: %s\n",
		    id, mem_path, strerror(errno));

	fclose(mem_file);
	free(mem_path);
}

bool cbmem_sysfs_get_cbmem_entry(uint32_t id, uint8_t **buf_out, size_t *size_out)
{
	uint64_t addr;
	size_t size;

	if (!cbmem_sysfs_probe_cbmem_entry(id, &addr, &size)) {
		debug("CBMEM entry id: %#x not found.\n", id);
		return false;
	}

	fetch_cbmem_entry(id, size, buf_out);
	*size_out = size;
	return true;
}

void cbmem_sysfs_free_entry(uint8_t *buf)
{
	free(buf);
}

bool cbmem_sysfs_write_cbmem_entry(uint32_t id, uint8_t *buf, size_t buf_size)
{
	char *mem_path = NULL;
	uint64_t addr;
	size_t size;

	if (!cbmem_sysfs_probe_cbmem_entry(id, &addr, &size)) {
		debug("Unable to find CBMEM entry id: %#x\n", id);
		return false;
	}

	if (buf_size > size)
		die("Attempting to write %zu bytes to CBMEM entry id: %#x of %zu bytes. Operation not possible.\n",
		    buf_size, id, size);

	if (!new_sysfs_path(id, CBMEM_SYSFS_PATH_MEM, &mem_path))
		die("Unable to create path to memory mapped to the CBMEM entry id: %#x. Error: %s\n",
		    id, strerror(errno));

	FILE *mem_file = fopen(mem_path, "rb+");
	if (!mem_file)
		die("Unable to open mem file for CBMEM entry id: %#x at %s. Error: %s\n", id,
		    mem_path, strerror(errno));

	if (fwrite(buf, 1, buf_size, mem_file) != buf_size)
		die("Unable to correctly write memory of CBMEM entry id: %#x at %s. Error: %s\n",
		    id, mem_path, strerror(errno));

	fclose(mem_file);
	free(mem_path);

	return true;
}

void cbmem_sysfs_foreach_cbmem_entry(cbmem_iterator_callback cb, void *data)
{
	DIR *dir;
	struct dirent *entry;

	if ((dir = opendir(CBMEM_SYSFS_BASE_DIR)) == NULL)
		die("Unable to open directory containing CBMEM entries. Path: %s. Error: %s\n",
		    CBMEM_SYSFS_BASE_DIR, strerror(errno));

	while ((entry = readdir(dir)) != NULL) {
		debug("Checking path: %s%s\n", CBMEM_SYSFS_BASE_DIR, entry->d_name);
		/* Check directory name prefix */
		if (strncmp(entry->d_name, CBMEM_SYSFS_ENTRY_DIR_NAME_PREFIX,
			    strlen(CBMEM_SYSFS_ENTRY_DIR_NAME_PREFIX)) != 0)
			continue;

		uint32_t id = 0;
		/* Extract CBMEM entry id from the directory name.
		   If it fails, then directory is not a correct entry. */
		if (sscanf(entry->d_name, CBMEM_SYSFS_ENTRY_DIR_NAME_FMT, &id) != 1)
			continue;

		uint64_t addr = 0;
		uint64_t size = 0;
		uint8_t *buf;

		/* If entry was not found or previously matched directory
		   does not contain necessary files, then omit the entry. */
		if (!cbmem_sysfs_probe_cbmem_entry(id, &addr, &size))
			continue;

		fetch_cbmem_entry(id, size, &buf);

		debug("Invokign callback on %s%s\n", CBMEM_SYSFS_BASE_DIR, entry->d_name);

		const bool res = cb(id, addr, buf, size, data);

		free(buf);

		/* Finish iteration if callback requested it. */
		if (res)
			break;
	}

	closedir(dir);
}
