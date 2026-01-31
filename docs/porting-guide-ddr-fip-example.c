/*
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Example: load a FIP located in DDR at 0x80000000 (size 6 MiB)
 * using the TF-A I/O storage layer.
 */

#include <assert.h>
#include <errno.h>

#include <common/debug.h>
#include <common/tbbr/tbbr_img_def.h>
#include <drivers/io/io_driver.h>
#include <drivers/io/io_fip.h>
#include <drivers/io/io_memmap.h>
#include <drivers/io/io_storage.h>
#include <lib/utils.h>
#include <tools_share/firmware_image_package.h>
#include <tools_share/uuid.h>

#define FIP_DDR_BASE   ULL(0x80000000)
#define FIP_DDR_SIZE   ULL(0x00600000)

/* IO devices */
static const io_dev_connector_t *fip_dev_con;
static uintptr_t fip_dev_handle;
static const io_dev_connector_t *memmap_dev_con;
static uintptr_t memmap_dev_handle;

static const io_block_spec_t fip_block_spec = {
	.offset = FIP_DDR_BASE,
	.length = FIP_DDR_SIZE,
};

static const io_uuid_spec_t bl2_uuid_spec = {
	.uuid = UUID_TRUSTED_BOOT_FIRMWARE_BL2,
};

static const io_uuid_spec_t bl31_uuid_spec = {
	.uuid = UUID_EL3_RUNTIME_FIRMWARE_BL31,
};

static const io_uuid_spec_t bl32_uuid_spec = {
	.uuid = UUID_SECURE_PAYLOAD_BL32,
};

static const io_uuid_spec_t bl33_uuid_spec = {
	.uuid = UUID_NON_TRUSTED_FIRMWARE_BL33,
};

static const io_uuid_spec_t tb_fw_config_uuid_spec = {
	.uuid = UUID_TB_FW_CONFIG,
};

static const io_uuid_spec_t tos_fw_config_uuid_spec = {
	.uuid = UUID_TOS_FW_CONFIG,
};

struct plat_io_policy {
	uintptr_t *dev_handle;
	uintptr_t image_spec;
	int (*check)(const uintptr_t spec);
};

static int open_fip(const uintptr_t spec)
{
	int result;
	uintptr_t local_image_handle;

	result = io_dev_init(fip_dev_handle, (uintptr_t)FIP_IMAGE_ID);
	if (result == 0) {
		result = io_open(fip_dev_handle, spec, &local_image_handle);
		if (result == 0) {
			VERBOSE("Using FIP\n");
			io_close(local_image_handle);
		}
	}

	return result;
}

static int open_memmap(const uintptr_t spec)
{
	int result;
	uintptr_t local_image_handle;

	result = io_dev_init(memmap_dev_handle, (uintptr_t)NULL);
	if (result == 0) {
		result = io_open(memmap_dev_handle, spec, &local_image_handle);
		if (result == 0) {
			VERBOSE("Using memmap\n");
			io_close(local_image_handle);
		}
	}

	return result;
}

static const struct plat_io_policy policies[] = {
	[FIP_IMAGE_ID] = {
		.dev_handle = &memmap_dev_handle,
		.image_spec = (uintptr_t)&fip_block_spec,
		.check = open_memmap,
	},
	[BL2_IMAGE_ID] = {
		.dev_handle = &fip_dev_handle,
		.image_spec = (uintptr_t)&bl2_uuid_spec,
		.check = open_fip,
	},
	[BL31_IMAGE_ID] = {
		.dev_handle = &fip_dev_handle,
		.image_spec = (uintptr_t)&bl31_uuid_spec,
		.check = open_fip,
	},
	[BL32_IMAGE_ID] = {
		.dev_handle = &fip_dev_handle,
		.image_spec = (uintptr_t)&bl32_uuid_spec,
		.check = open_fip,
	},
	[BL33_IMAGE_ID] = {
		.dev_handle = &fip_dev_handle,
		.image_spec = (uintptr_t)&bl33_uuid_spec,
		.check = open_fip,
	},
	[TB_FW_CONFIG_ID] = {
		.dev_handle = &fip_dev_handle,
		.image_spec = (uintptr_t)&tb_fw_config_uuid_spec,
		.check = open_fip,
	},
	[TOS_FW_CONFIG_ID] = {
		.dev_handle = &fip_dev_handle,
		.image_spec = (uintptr_t)&tos_fw_config_uuid_spec,
		.check = open_fip,
	},
};

void ddr_fip_io_setup(void)
{
	int io_result;

	io_result = register_io_dev_fip(&fip_dev_con);
	if (io_result != 0) {
		panic();
	}

	io_result = register_io_dev_memmap(&memmap_dev_con);
	if (io_result != 0) {
		panic();
	}

	io_result = io_dev_open(fip_dev_con, (uintptr_t)NULL, &fip_dev_handle);
	if (io_result != 0) {
		panic();
	}

	io_result = io_dev_open(memmap_dev_con, (uintptr_t)NULL, &memmap_dev_handle);
	if (io_result != 0) {
		panic();
	}
}

int plat_get_image_source(unsigned int image_id,
			   uintptr_t *dev_handle,
			   uintptr_t *image_spec)
{
	const struct plat_io_policy *policy;
	int result;

	if (image_id >= ARRAY_SIZE(policies)) {
		return -ENOENT;
	}

	policy = &policies[image_id];
	if (policy->check == NULL) {
		return -ENOENT;
	}

	result = policy->check(policy->image_spec);
	if (result == 0) {
		*dev_handle = *(policy->dev_handle);
		*image_spec = policy->image_spec;
	}

	return result;
}
