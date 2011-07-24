/*
 *  linux/arch/arm/mach-comcas/core.c
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/sysdev.h>
#include <linux/interrupt.h>
#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <asm/system.h>
#include <mach/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach/irq.h>
#include <asm/mach/map.h>

#include "core.h"

#define COMCAS_REFCOUNTER	(__io_address (COMCAS_SYS_BASE) + COMCAS_SYS_24MHz_OFFSET)

/* used by entry-macro.S */
void __iomem *gic_cpu_base_addr;

static void timer_set_mode (enum clock_event_mode mode, struct clock_event_device *clk)
{
    unsigned long ctrl;

    switch(mode)
    {
    case CLOCK_EVT_MODE_ONESHOT:
        writel (1, __io_address (COMCAS_TIMER0) + TIMER_BOOL_ONE_SHOT_OFFSET);
        ctrl = 10000000;
        break;
    case CLOCK_EVT_MODE_PERIODIC:
        writel (0, __io_address (COMCAS_TIMER0) + TIMER_BOOL_ONE_SHOT_OFFSET);
        /* period set, and timer enabled in 'next_event' hook */
        ctrl = 10000000;
        break;
    case CLOCK_EVT_MODE_UNUSED:
    case CLOCK_EVT_MODE_SHUTDOWN:
    default:
        ctrl = 0;
    }

    writel (ctrl, __io_address (COMCAS_TIMER0) + TIMER_INT_PERIOD_OFFSET);
}

static int timer_set_next_event (unsigned long evt,
    struct clock_event_device *unused)
{
    return 0;
}

static struct clock_event_device timer0_clockevent =
{
    .name           = "timer0",
    .shift          = 32,
    .features       = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
    .set_mode       = timer_set_mode,
    .set_next_event = timer_set_next_event,
    .rating         = 300,
    .cpumask        = cpu_all_mask,
};

static void __init comcas_clockevents_init (unsigned int timer_irq)
{
	timer0_clockevent.irq = timer_irq;
	timer0_clockevent.mult =
		div_sc (1000000, NSEC_PER_SEC, timer0_clockevent.shift);
	timer0_clockevent.max_delta_ns =
		clockevent_delta2ns (0xffffffff, &timer0_clockevent);
	timer0_clockevent.min_delta_ns =
		clockevent_delta2ns (0xf, &timer0_clockevent);

	clockevents_register_device (&timer0_clockevent);
}

/*
 * IRQ handler for the timer
 */
static irqreturn_t comcas_timer_interrupt (int irq, void *dev_id)
{
    struct clock_event_device *evt = &timer0_clockevent;

    /* clear the interrupt */
    writel (TIMER_INT_ACK_VALUE, __io_address (COMCAS_TIMER0) + TIMER_INT_ACK_OFFSET);

//§§    printk ("TIMER INTERRUPT\n");

    evt->event_handler (evt);

    return IRQ_HANDLED;
}

static struct irqaction comcas_timer_irq =
{
    .name       = "Comcas Timer Tick",
    .flags      = IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
    .handler    = comcas_timer_interrupt,
};

/*
 * Set up the clock source and clock events devices
 */
void __init comcas_timer_init (void)
{

    /*
    * Initialise to a known state (all timers off)
    */
    writel (0, __io_address (COMCAS_TIMER0) + TIMER_INT_PERIOD_OFFSET);

    /* 
    * Make irqs happen for the system timer
    */
    setup_irq (IRQ_TIMER0, &comcas_timer_irq);
    comcas_clockevents_init (IRQ_TIMER0);
}
