/* SPDX-License-Identifier: GPL-2.0-only */

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include <commonlib/bsd/cbmem_id.h>
#include <commonlib/bsd/helpers.h>

extern int cbmem_util_verbose;

#define debug(x...)                                                            \
	do {                                                                   \
		if (cbmem_util_verbose > 1)                                    \
			printf("[%s:%d %s()] ", __FILE__, __LINE__, __func__); \
		if (cbmem_util_verbose)                                        \
			printf(x);                                             \
	} while (0)

#define die(x...)                   \
	do {                        \
		fprintf(stderr, x); \
		abort();            \
	} while (0)

void hexdump(const uintptr_t start_address, const uint8_t *buf, const int length);

struct cbmem_console {
	uint32_t size;
	uint32_t cursor;
	uint8_t body[];
} __packed;

#define CBMC_CURSOR_MASK ((1 << 28) - 1)
#define CBMC_OVERFLOW    (1 << 31)

void cbmem_print_entry(int n, uint32_t id, uint64_t base, uint64_t size);

/**
 * Function pointer type used by CBMEM foreach iteration calls.
 *
 * @param id CBMEM_ID_* value.
 * @param physical_address CBMEM entry address in physical memory.
 * @param buf heap-allocated bufer with CBMEM entry contents.
 * @param size size of CBMEM entry in bytes.
 * @param data callback-specific context data.
 *
 * @returns true if iteration should finish, false if it should continue.
 */
typedef bool (*cbmem_iterator_callback)(const uint32_t id, const uint64_t physical_address,
					const uint8_t *buf, const size_t size, void *data);

/**
 * Get a coreboot table entry as an allocated buffer.
 *
 * @param tag LB_TAG_* value.
 * @param buf_out return pointer for the allocated buffer containing entry contents.
 * @param size_out size of returned buffer.
 *
 * Aborts (via abort()) on failure.
 */
void cbmem_get_lb_table_entry(uint32_t tag, uint8_t **buf_out, size_t *size_out);

/* API for accessing CBMEM via /dev/mem */

/**
 * Initialize the driver.
 *
 * @param writeable tries to map CBMEM in R/W mode.
 *
 * @returns true on success, false otherwise.
 */
bool cbmem_devmem_init(bool writeable);

/**
 * Cleanup and terminate the driver. **MUST** be called if cbmem_devmem_init succeeded before.
 */
void cbmem_devmem_terminate(void);

/**
 * Check if CBMEM entry exists. If it does, then return address and size.
 *
 * @param id CBMEM_ID_* value.
 * @param addr_out pointer to the output buffer for entry address in physical memory.
 * @param size_out pointer to the output buffer for entry size.
 *
 * @returns true if entry exists, false otherwise.
 */
bool cbmem_devmem_probe_cbmem_entry(uint32_t id, uint64_t *addr_out, size_t *size_out);

/**
 * Get CBMEM entry as an allocated buffer.
 *
 * @param id CBMEM_ID_* value.
 * @param buf_out return pointer for the allocated buffer containing entry contents.
 * @param size_out size of returned buffer.
 *
 * @returns true on success, false otherwise.
 */
bool cbmem_devmem_get_cbmem_entry(uint32_t id, uint8_t **buf_out, size_t *size_out);

/**
 * Free buffer allocated by cbmem_devmem_get_cbmem_entry().
 */
void cbmem_devmem_free_entry(uint8_t *buf);

/**
 * Write provided buffer contents to the CBMEM entry.
 *
 * @param id CBMEM_ID_* value.
 * @param buf pointer to the source buffer.
 * @param buf_size size of the source buffer.
 *
 * @returns true on success, false otherwise.
 */
bool cbmem_devmem_write_cbmem_entry(uint32_t id, uint8_t *buf, size_t buf_size);

/**
 * Backend-specific function iterating over CBMEM entries.
 *
 * @param cb user callback function to call during iteration.
 * @param data pointer to the context data for the callback.
 */
void cbmem_devmem_foreach_cbmem_entry(cbmem_iterator_callback cb, void *data);
