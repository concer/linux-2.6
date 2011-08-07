#ifndef __ASM_ARM_ARCH_IO_H
#define __ASM_ARM_ARCH_IO_H

#define IO_SPACE_LIMIT 0xffffffff

static inline void __iomem *__io (unsigned long addr)
{
	return (void __iomem *) addr;
}

#define __io(a)         __io(a)
#define __mem_pci(a)  (a)

#endif
