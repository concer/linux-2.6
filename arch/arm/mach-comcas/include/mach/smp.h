#ifndef ASMARM_ARCH_SMP_H
#define ASMARM_ARCH_SMP_H

#define hard_smp_processor_id() \
    ({ \
        unsigned int cpunum; \
        __asm__("mrc p15, 0, %0, c0, c0, 5" \
            : "=r" (cpunum)); \
        cpunum &= 0x0F; \
    })

/*
 * We use IRQ1 as the IPI
 */
static inline void smp_cross_call(const struct cpumask *callmap)
{
    extern void comcas_raise_softirq(const struct cpumask *cpumask);

    comcas_raise_softirq(callmap);
}

/*
 * Do nothing on MPcore.
 */
static inline void smp_cross_call_done(cpumask_t callmap)
{
}

#endif
