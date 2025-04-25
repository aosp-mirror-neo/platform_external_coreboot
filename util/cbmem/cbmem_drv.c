/* SPDX-License-Identifier: GPL-2.0-only */

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "cbmem_util.h"

/* Zero means no backend is active. */
static enum cbmem_drv_backend_type active_backend = 0;

static bool init_specific_backend(enum cbmem_drv_backend_type backend, bool writeable)
{
	bool ret = false;

	switch (backend) {
	case CBMEM_DRV_BACKEND_DEVMEM:
		ret = cbmem_devmem_init(writeable);
		break;
	case CBMEM_DRV_BACKEND_SYSFS:
		ret = cbmem_sysfs_init();
		break;
	default:
		die("Unsupported backend selected. Terminating.\n");
	}

	if (ret) {
		active_backend = backend;
		switch (active_backend) {
		case CBMEM_DRV_BACKEND_DEVMEM:
			debug("Initialized CBMEM backend: devmem\n");
			break;
		case CBMEM_DRV_BACKEND_SYSFS:
			debug("Initialized CBMEM backend: sysfs\n");
			break;
		default: /* unreachable */
			break;
		}
	}
	return ret;
}

bool cbmem_drv_init(enum cbmem_drv_backend_type backend, bool writeable)
{
	if (backend != CBMEM_DRV_BACKEND_ANY)
		return init_specific_backend(backend, writeable);

	/* First try SYSFS backend as it's more secure. */
	if (!init_specific_backend(CBMEM_DRV_BACKEND_SYSFS, writeable))
		return init_specific_backend(CBMEM_DRV_BACKEND_DEVMEM, writeable);

	return true;
}

void cbmem_drv_terminate(void)
{
	/* Only /dev/mem driver requires termination. */
	if (active_backend == CBMEM_DRV_BACKEND_DEVMEM)
		cbmem_devmem_terminate();

	active_backend = CBMEM_DRV_BACKEND_ANY;
}

bool cbmem_drv_probe_cbmem_entry(uint32_t id, uint64_t *addr_out, size_t *size_out)
{
	switch (active_backend) {
	case CBMEM_DRV_BACKEND_ANY:
		die("No active/initialized CBMEM backend detected. Terminating.\n");
	case CBMEM_DRV_BACKEND_DEVMEM:
		return cbmem_devmem_probe_cbmem_entry(id, addr_out, size_out);
	case CBMEM_DRV_BACKEND_SYSFS:
		return cbmem_sysfs_probe_cbmem_entry(id, addr_out, size_out);
	}
	return false;
}

bool cbmem_drv_get_cbmem_entry(uint32_t id, uint8_t **buf_out, size_t *size_out)
{
	switch (active_backend) {
	case CBMEM_DRV_BACKEND_ANY:
		die("No active/initialized CBMEM backend detected. Terminating.\n");
	case CBMEM_DRV_BACKEND_DEVMEM:
		return cbmem_devmem_get_cbmem_entry(id, buf_out, size_out);
	case CBMEM_DRV_BACKEND_SYSFS:
		return cbmem_sysfs_get_cbmem_entry(id, buf_out, size_out);
	}
	return false;
}

void cbmem_drv_free_entry(uint8_t *buf)
{
	switch (active_backend) {
	case CBMEM_DRV_BACKEND_ANY:
		die("No active/initialized CBMEM backend detected. Terminating.\n");
	case CBMEM_DRV_BACKEND_DEVMEM:
		cbmem_devmem_free_entry(buf);
		break;
	case CBMEM_DRV_BACKEND_SYSFS:
		cbmem_sysfs_free_entry(buf);
		break;
	}
}

bool cbmem_drv_write_cbmem_entry(uint32_t id, uint8_t *buf, size_t buf_size)
{
	switch (active_backend) {
	case CBMEM_DRV_BACKEND_DEVMEM:
		return cbmem_devmem_write_cbmem_entry(id, buf, buf_size);
	case CBMEM_DRV_BACKEND_SYSFS:
		return cbmem_sysfs_write_cbmem_entry(id, buf, buf_size);
	default:
		die("No active/initialized CBMEM backend detected. Terminating.\n");
	}
}

void cbmem_drv_foreach_cbmem_entry(cbmem_iterator_callback cb, void *data)
{
	switch (active_backend) {
	case CBMEM_DRV_BACKEND_DEVMEM:
		cbmem_devmem_foreach_cbmem_entry(cb, data);
		break;
	case CBMEM_DRV_BACKEND_SYSFS:
		cbmem_sysfs_foreach_cbmem_entry(cb, data);
		break;
	default:
		die("No active/initialized CBMEM backend detected. Terminating.\n");
		break;
	}
}
