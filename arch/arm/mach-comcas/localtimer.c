/*
 *  linux/arch/arm/mach-comcas/localtimer.c
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/smp.h>
#include <linux/jiffies.h>
#include <linux/percpu.h>
#include <linux/clockchips.h>
#include <linux/irq.h>
#include <mach/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>

extern void __iomem *gic_cpu_base_addr;


