/*
 *  linux/arch/arm/mach-comcas/platsmp.c
 *
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/smp.h>

#include <asm/cacheflush.h>
#include <mach/hardware.h>
#include <asm/io.h>
#include <asm/mach-types.h>
#include <asm/smp_scu.h>
#include <asm/hardware/gic.h>

#include <mach/board-comcas.h>
#include <mach/scu.h>

/*
 * control for which core is the next to come out of the secondary
 * boot "holding pen"
 */
volatile int __cpuinitdata pen_release = -1;

static unsigned int __init get_core_count (void)
{
    unsigned int        ncores;
    void __iomem        *qemu_base;

    qemu_base = __io_address (COMCAS_QEMU_ADDR_BASE);
    ncores = __raw_readl (qemu_base + GET_SYSTEMC_NO_CPUS);

	return ncores;
}

static DEFINE_SPINLOCK(boot_lock);

/*
 * Write pen_release in a way that is guaranteed to be visible to all
 * observers, irrespective of whether they're taking part in coherency
 * or not.  This is necessary for the hotplug code to work reliably.
 */
static void __cpuinit write_pen_release(int val)
{
	pen_release = val;
	smp_wmb();
	__cpuc_flush_dcache_area((void *)&pen_release, sizeof(pen_release));
	outer_clean_range(__pa(&pen_release), __pa(&pen_release + 1));
}

void __cpuinit platform_secondary_init (unsigned int cpu)
{
    /*
    * the primary core may have used a "cross call" soft interrupt
    * to get this processor out of WFI in the BootMonitor - make
    * sure that we are no longer being sent this soft interrupt
    */
    smp_cross_call_done (cpumask_of_cpu (cpu));

    /*
    * if any interrupts are already enabled for the primary
    * core (e.g. timer irq), then they will not have been enabled
    * for us: do so
    */
    //§gic_cpu_init (0, __io_address (COMCAS_GIC_CPU_BASE));

    /*
    * let the primary processor know we're out of the
    * pen, then head off into the C entry point
    */
    write_pen_release(-1);

    /*
    * Synchronise with the boot thread.
    */
    spin_lock (&boot_lock);
    spin_unlock (&boot_lock);
}

int __cpuinit boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	unsigned long timeout;

	/*
	 * set synchronisation state between this boot processor
	 * and the secondary one
	 */
	spin_lock(&boot_lock);

	/*
	 * The secondary processor is waiting to be released from
	 * the holding pen - release it, then wait for it to flag
	 * that it has been released by resetting pen_release.
	 *
	 * Note that "pen_release" is the hardware CPU ID, whereas
	 * "cpu" is Linux's internal ID.
	 */
	pen_release = cpu;
	flush_cache_all();

	/*
	 * Send the secondary CPU a soft interrupt, thereby causing
	 * the boot monitor to read the system wide flags register,
	 * and branch to the address found there.
	 */
	smp_cross_call(cpumask_of(cpu), 1);

	timeout = jiffies + (1 * HZ);
	while(time_before(jiffies, timeout))
    {
		smp_rmb ();
		if(pen_release == -1)
			break;

		udelay(10);
	}

	/*
	 * now the secondary core is starting up let it run its
	 * calibrations, then wait for it to finish
	 */
	spin_unlock(&boot_lock);

	return pen_release != -1 ? -ENOSYS : 0;
}

/*
 * Initialise the CPU possible map early - this describes the CPUs
 * which may be present or become present in the system.
 */
void __init smp_init_cpus(void)
{
	unsigned int i, ncores = get_core_count();

	for(i = 0; i < ncores; i++)
		cpu_set(i, cpu_possible_map);

	set_smp_cross_call(gic_raise_softirq);
}

void __init platform_smp_prepare_cpus(unsigned int max_cpus)
{
    int i;

    scu_enable(scu_base_addr());
    /*
     * Write the address of secondary startup into the
     * system-wide flags register. The BootMonitor waits
     * until it receives a soft interrupt, and then the
     * secondary CPU branches to this address.
     */
    __raw_writel(virt_to_phys(comcas_secondary_startup),
	    __io_address(COMCAS_QEMU_ADDR_BASE) + 0x84);
}
