#ifndef __ASM_ARCH_SYSTEM_H
#define __ASM_ARCH_SYSTEM_H

#include <mach/hardware.h>
#include <asm/io.h>
#include <mach/platform.h>

static inline void arch_idle (void)
{
	/*
	 * This should do all the clock switching
	 * and wait for interrupt tricks
	 */
	cpu_do_idle ();
}

static inline void arch_reset (char mode, const char *cmd)
{
//	void __iomem *hdr_ctrl = __io_address (COMCAS_QEMU_ADDR_BASE) + COMCAS_SYS_RESETCTL_OFFSET;
//	__raw_writel(val, hdr_ctrl);
}

#endif
