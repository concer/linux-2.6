#ifndef ASMARM_ARCH_SMP_H
#define ASMARM_ARCH_SMP_H

#include <asm/smp_mpidr.h>

/*
 * We use IRQ1 as the IPI
 */
static inline void smp_cross_call(const struct cpumask *callmap, int ipi)
{
	extern void comcas_raise_softirq(const struct cpumask *cpumask, int ipi);

    comcas_raise_softirq(callmap, ipi);
}

/*
 * Do nothing on MPcore.
 */
static inline void smp_cross_call_done(cpumask_t callmap)
{
}

#endif
