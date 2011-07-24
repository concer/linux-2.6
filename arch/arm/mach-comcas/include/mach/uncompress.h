#include <mach/hardware.h>
#include <asm/mach-types.h>

#include <mach/board-comcas.h>

#define UART_DR(base)      (*(volatile unsigned char *)((base) + 0x00))
#define UART_FR(base)      (*(volatile unsigned char *)((base) + 0x18))

/*
 * Return the UART base address
 */
static inline unsigned long get_uart_base (void)
{
    return COMCAS_UART0_BASE;
}

/*
 * This does not append a newline
 */
static inline void putc (int c)
{
	unsigned long base = get_uart_base();

	while (UART_FR(base) & (1 << 5))
		barrier();

	UART_DR(base) = c;
}

static inline void flush(void)
{
	unsigned long base = get_uart_base();

	while (UART_FR(base) & (1 << 3))
		barrier();
}

/*
 * nothing to do
 */
#define arch_decomp_setup()
#define arch_decomp_wdog()
