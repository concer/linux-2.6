#ifndef __ASM_ARCH_PLATFORM_H
#define __ASM_ARCH_PLATFORM_H

/*
 * Memory definitions
 */
#define COMCAS_BOOT_ROM_LO                  0x30000000		/* DoC Base (64Mb)...*/
#define COMCAS_BOOT_ROM_HI                  0x30000000
#define COMCAS_BOOT_ROM_BASE                COMCAS_BOOT_ROM_HI	 /*  Normal position */
#define COMCAS_BOOT_ROM_SIZE                SZ_64M

#define COMCAS_SSRAM_BASE                   /* COMCAS_SSMC_BASE ? */
#define COMCAS_SSRAM_SIZE                   SZ_2M

/* 
 *  SDRAM
 */
#define COMCAS_SDRAM_BASE                   0x00000000


/*
 * System controller bit assignment
 */
#define COMCAS_REFCLK                       0
#define COMCAS_TIMCLK                       1

#define MAX_PERIOD                          699050
#define TICKS_PER_uSEC                      1
/* 
 *  These are useconds NOT ticks.  
 * 
 */
#define mSEC_1                              1000
#define mSEC_5                              (mSEC_1 * 5)
#define mSEC_10                             (mSEC_1 * 10)
#define mSEC_25                             (mSEC_1 * 25)
#define SEC_1                               (mSEC_1 * 1000)

/* ------------------------------------------------------------------------
 *  COMCAS Registers
 * ------------------------------------------------------------------------
 * 
 */

#define COMCAS_QEMU_ADDR_BASE               0x82000000
#define COMCAS_UART0_BASE                   0xA8000000
#define COMCAS_TIMER0                       0xA4000000

#define QEMU_INT_ENABLE_OFFSET              0x0040
#define QEMU_GET_MAX_INT_PENDING            0x0080
#define TIMER_INT_PERIOD_OFFSET             0x0000
#define TIMER_INT_ACK_OFFSET                0x0000
#define TIMER_INT_ACK_VALUE                 0xFFFFFFFF
#define TIMER_BOOL_ONE_SHOT_OFFSET          0x0008

#define IRQ_LOCAL_TIMER                     29
#define IRQ_TIMER0                          32
#define IRQ_UART0                           33

#endif	/* __ASM_ARCH_PLATFORM_H */
