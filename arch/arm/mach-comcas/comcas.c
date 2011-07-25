/*
 *  linux/arch/arm/mach-comcas/comcas.c
*/

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/sysdev.h>
#include <mach/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <linux/irq.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>
#include <mach/irqs.h>

#include "core.h"

static DEFINE_SPINLOCK(irq_controller_lock);

static int              implemented_irqs [] =    {IRQ_LOCAL_TIMER, IRQ_TIMER0, IRQ_UART0, 34, 35};
static unsigned long    mask_implemented_irqs[] = {0, 1, 2, 4, 8};
static int              nimplemented_irqs = sizeof (implemented_irqs) / sizeof (int);

static struct map_desc comcas_io_desc[] __initdata =
{
    {
        .virtual    = IO_ADDRESS (COMCAS_QEMU_ADDR_BASE),
        .pfn        = __phys_to_pfn (COMCAS_QEMU_ADDR_BASE),
        .length     = SZ_4K,
        .type       = MT_DEVICE,
    },
    {
        .virtual    = IO_ADDRESS (COMCAS_TIMER0),
        .pfn        = __phys_to_pfn (COMCAS_TIMER0),
        .length     = SZ_4K,
        .type       = MT_DEVICE,
    },
    {
        .virtual    = IO_ADDRESS (COMCAS_UART0_BASE),
        .pfn        = __phys_to_pfn (COMCAS_UART0_BASE),
        .length     = SZ_4K,
        .type       = MT_DEVICE,
    },
};

extern void __iomem *gic_cpu_base_addr;

static void __init comcas_map_io (void)
{
    iotable_init (comcas_io_desc, ARRAY_SIZE (comcas_io_desc));

    gic_cpu_base_addr = __io_address (COMCAS_QEMU_ADDR_BASE);
}

static inline unsigned long comcas_get_gic_irq_mask (int irq)
{
    int                 i;

    for (i = 0; i < nimplemented_irqs; i++)
        if (irq == implemented_irqs[i])
            break;
    if (i >= nimplemented_irqs)
        return 0;

    return mask_implemented_irqs[i];

}

static void comcas_gic_ack_irq (unsigned int irq)
{
}

void comcas_gic_mask_irq (unsigned int irq)
{
    unsigned long       mask, val;

    mask = comcas_get_gic_irq_mask (irq);
    if (!mask)
        return;

    spin_lock(&irq_controller_lock);
    val = readl (gic_cpu_base_addr + QEMU_INT_ENABLE_OFFSET);
    writel (~mask & val, gic_cpu_base_addr + QEMU_INT_ENABLE_OFFSET);
    spin_unlock(&irq_controller_lock);
}

void comcas_gic_unmask_irq (unsigned int irq)
{
    unsigned long       mask, val;

    mask = comcas_get_gic_irq_mask (irq);
    if (!mask)
        return;

    spin_lock (&irq_controller_lock);
    val = readl (gic_cpu_base_addr + QEMU_INT_ENABLE_OFFSET);
    writel (mask | val, gic_cpu_base_addr + QEMU_INT_ENABLE_OFFSET);
    spin_unlock (&irq_controller_lock);
}

#ifdef CONFIG_SMP
static int comcas_gic_set_cpu (unsigned int irq, const struct cpumask *dest)
{
	 return 0;
}
#endif

static struct irq_chip comcas_gic_chip =
{
    .name       = "COMCAS_GIC",
    .ack        = comcas_gic_ack_irq,
    .mask       = comcas_gic_mask_irq,
    .unmask     = comcas_gic_unmask_irq,
    #ifdef CONFIG_SMP
    .set_affinity   = comcas_gic_set_cpu,
    #endif
};

static void __init comcas_gic_init (void)
{
    int                 i, irq;

    //disable all irqs
    writel (0, gic_cpu_base_addr + QEMU_INT_ENABLE_OFFSET);

    /*
     * Setup the Linux IRQ subsystem.
     */
    for (i = 0; i < nimplemented_irqs; i++)
    {
        irq = implemented_irqs[i];
        set_irq_chip (irq, &comcas_gic_chip);
        set_irq_chip_data (irq, 0);
        set_irq_handler (irq, handle_level_irq);
        set_irq_flags (irq, IRQF_VALID | IRQF_PROBE);
    }
}

static void __init gic_init_irq (void)
{
    comcas_gic_init ();
}

static struct sys_timer comcas_timer = 
{
    .init		= comcas_timer_init,
};

static void __init comcas_init (void)
{
}

MACHINE_START(COMCAS, "COMCAS")
	.boot_params	= 0x00000100,
	.map_io		= comcas_map_io,
	.init_irq	= gic_init_irq,
	.timer		= &comcas_timer,
	.init_machine	= comcas_init,
MACHINE_END

void comcas_raise_softirq (const struct cpumask *cpumask)
{
    unsigned long map = *cpus_addr(*cpumask);

    __raw_writel (map, __io_address (COMCAS_QEMU_ADDR_BASE) + 0x88);
}
