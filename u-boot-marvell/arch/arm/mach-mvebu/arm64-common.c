/*
 * Copyright (C) 2016 Stefan Roese <sr@denx.de>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <fdtdec.h>
#include <linux/libfdt.h>
#include <pci.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/arch/cpu.h>
#include <asm/arch/soc.h>
#include <asm/armv8/mmu.h>
#include <power/regulator.h>
#include <mach/fw_info.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * Not all memory is mapped in the MMU. So we need to restrict the
 * memory size so that U-Boot does not try to access it. Also, the
 * internal registers are located at 0xf000.0000 - 0xffff.ffff.
 * Currently only 2GiB are mapped for system memory. This is what
 * we pass to the U-Boot subsystem here.
 */
#define USABLE_RAM_SIZE		0x80000000

ulong board_get_usable_ram_top(ulong total_size)
{
	if (gd->ram_size > USABLE_RAM_SIZE)
		return USABLE_RAM_SIZE;

	return gd->ram_size;
}

/*
 * On ARMv8, MBus is not configured in U-Boot. To enable compilation
 * of the already implemented drivers, lets add a dummy version of
 * this function so that linking does not fail.
 */
const struct mbus_dram_target_info *mvebu_mbus_dram_info(void)
{
	return NULL;
}

/* DRAM init code ... */

__weak int mvebu_dram_init(void)
{
	return fdtdec_setup_memory_size();
}

__weak int mvebu_dram_init_banksize(void)
{
	return fdtdec_setup_memory_banksize();
}

int dram_init_banksize(void)
{
	return mvebu_dram_init_banksize();
}

int dram_init(void)
{
	if (mvebu_dram_init() != 0)
		return -EINVAL;

	return 0;
}

int arch_cpu_init(void)
{
	/* Nothing to do (yet) */
	return 0;
}

int arch_early_init_r(void)
{
	struct udevice *dev;
	int ret;
	int i;

	/*
	 * Loop over all MISC uclass drivers to call the comphy code
	 * and init all CP110 devices enabled in the DT
	 */
	i = 0;
	while (1) {
		/* Call the comphy code via the MISC uclass driver */
		ret = uclass_get_device(UCLASS_MISC, i++, &dev);

		/* We're done, once no further CP110 device is found */
		if (ret)
			break;
	}

	i = 0;
	while (1) {
		/* Call the pinctrl code via the PINCTRL uclass driver */
		ret = uclass_get_device(UCLASS_PINCTRL, i++, &dev);

		/* We're done, once no further CP110 device is found */
		if (ret)
			break;
	}

	/* Cause the SATA device to do its early init */
	uclass_first_device(UCLASS_AHCI, &dev);

#ifdef CONFIG_DM_PCI
	/* Trigger PCIe devices detection */
	pci_init();
#endif

	return 0;
}

void plat_do_sync(void)
{
	u32 far, el;

	el = current_el();

	if (el == 1)
		asm volatile("mrs %0, far_el1" : "=r" (far));
	else if (el == 2)
		asm volatile("mrs %0, far_el2" : "=r" (far));
	else
		asm volatile("mrs %0, far_el3" : "=r" (far));

	if (far >= ATF_REGION_START && far <= ATF_REGION_END) {
		pr_err("\n\tAttempt to access RT service or TEE region (addr: 0x%x, el%d)\n",
		       far, el);
		pr_err("\tDo not use address range 0x%x-0x%x\n\n",
		       ATF_REGION_START, ATF_REGION_END);
	}
}
