/*
 * linux/arch/arm/mach-at91/board-whiterabbit-mch.c
 *
 *  Copyright (c) 2009 Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *  Copyright (c) 2009 Emilio G. Cota <cota@braap.org>
 *
 *  Derived from arch/arm/mach-at91/board-sam9263ek.c
 *	Copyright (C) 2005 SAN People
 *	Copyright (C) 2007 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/spi/flash.h>
#include <linux/mtd/mtd.h>
#include <linux/spi/spi.h>
#include <linux/init.h>

#include <asm/mach-types.h>
#include <asm/setup.h>
#include <asm/irq.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <mach/at91sam9_smc.h>
#include <mach/at91_shdwc.h>
#include <mach/board.h>
#include <mach/gpio.h>

#include "sam9_smc.h"
#include "generic.h"

static void __init wr_mch_map_io(void)
{
	/* Initialise processor */
	at91sam9263_initialize(18432000);

	/* DGBU on ttyS0. (Rx & Tx only) */
	at91_register_uart(0, 0, 0);

	/* USART0 on ttyS1. (Rx, Tx, RTS, CTS) */
	at91_register_uart(AT91SAM9263_ID_US0, 1,
			ATMEL_UART_CTS | ATMEL_UART_RTS);

	/* set serial console to ttyS0 (ie, DBGU) */
	at91_set_serial_console(0);
}

static void __init wr_mch_init_irq(void)
{
	at91sam9263_init_interrupts(NULL);
}

/*
 * SPI devices: MTD over SPI
 */
static struct mtd_partition wr_mch_dataflash_partitions[] __initdata = {
	{
		.name		= "at91bootstrap",
		.offset		= 0,
		.size		= 0x4200,
		.mask_flags	= MTD_WRITEABLE
	},
	{
		.name		= "uboot-environ",
		.offset		= MTDPART_OFS_NXTBLK,
		.size		= 0x4200
	},
	{
		.name		= "uboot-loader",
		.offset		= MTDPART_OFS_NXTBLK,
		.size		= 0x42000 - 0x8400,
		.mask_flags	= MTD_WRITEABLE
	},
	{
		.name		= "kernel",
		.offset		= MTDPART_OFS_NXTBLK,
		.size		= 0x252000 - 0x42000
	},
	{
		.name		= "kernel",
		.offset		= MTDPART_OFS_NXTBLK,
		.size		= MTDPART_SIZ_FULL
	}
};

static struct flash_platform_data wr_mch_flash_platform __initdata = {
	.name		= "SPI dataflash",
	.parts		= wr_mch_dataflash_partitions,
	.nr_parts	= ARRAY_SIZE(wr_mch_dataflash_partitions)
};

static struct spi_board_info wr_mch_spi_devices[] __initdata = {
	{
		.modalias	= "mtd_dataflash",
		.chip_select	= 0,
		.max_speed_hz	= 5 * 1000 * 1000,
		.bus_num	= 0,
		.platform_data	= &wr_mch_flash_platform
	}
};

/*
 * MACB Ethernet device
 */
static struct at91_eth_data wr_mch_macb_data __initdata = {
	.phy_irq_pin	= AT91_PIN_PE31,
	.is_rmii	= 1,
};

/*
 * WhiteRabbit NIC (wr-nic) stub
 */
#define WR_MCH_FPGA_BASE	0x70000000
#define WR_MCH_CS0_SIZE		0x200000
#define WR_MCH_NIC_BASE		((WR_MCH_FPGA_BASE) + 0x140000)
#define WR_MCH_NIC_SIZE		0x40000

static struct resource wr_mch_nic_resources[] = {
	[0] = {
		.start	= WR_MCH_NIC_BASE,
		.end	= WR_MCH_NIC_BASE + WR_MCH_NIC_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9263_ID_IRQ0,
		.end	= AT91SAM9263_ID_IRQ0,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= WR_MCH_FPGA_BASE,
		.end	= WR_MCH_FPGA_BASE + WR_MCH_CS0_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static u64 wr_mch_nic_dmamask = DMA_BIT_MASK(32);

static struct platform_device wr_mch_nic_device = {
	.name		= "wr-mch",
	.id		= -1,
	.dev		= {
			.dma_mask		= &wr_mch_nic_dmamask,
			.coherent_dma_mask	= DMA_BIT_MASK(32),
			.platform_data		= NULL,
	},
	.resource	= wr_mch_nic_resources,
	.num_resources	= ARRAY_SIZE(wr_mch_nic_resources),
};

static void __init wr_mch_add_device_nic(void)
{
	/* @todo: init code for the NIC here (IRQ0, etc) */
	at91_set_B_periph(AT91_PIN_PA14, 0);

	platform_device_register(&wr_mch_nic_device);
}

/*
 * NAND flash
 */
static struct mtd_partition wr_mch_nand_partitions[] __initdata = {
	{
		.name	= "Main",
		.offset	= 0,
		.size	= SZ_32M,
	},
};

static struct mtd_partition * __init nand_partitions(int size, int *nr_parts)
{
	*nr_parts = ARRAY_SIZE(wr_mch_nand_partitions);
	return wr_mch_nand_partitions;
}

static struct atmel_nand_data wr_mch_nand_data __initdata = {
	.ale		= 21,
	.cle		= 22,
	/* det_pin not connected */
	.rdy_pin	= AT91_PIN_PE19,
	.enable_pin	= AT91_PIN_PD15,
	.partition_info	= nand_partitions,
#if defined(CONFIG_MTD_NAND_ATMEL_BUSWIDTH_16)
	.bus_width_16	= 1,
#else
	.bus_width_16	= 0,
#endif
};

static struct sam9_smc_config wr_mch_nand_smc_config __initdata = {
	.ncs_read_setup		= 0,
	.nrd_setup		= 1,
	.ncs_write_setup	= 0,
	.nwe_setup		= 1,

	.ncs_read_pulse		= 3,
	.nrd_pulse		= 3,
	.ncs_write_pulse	= 3,
	.nwe_pulse		= 3,

	.read_cycle		= 5,
	.write_cycle		= 5,

	.mode			= AT91_SMC_READMODE |
				  AT91_SMC_WRITEMODE |
				  AT91_SMC_EXNWMODE_DISABLE,
	.tdf_cycles		= 2,
};

static void __init wr_mch_add_device_nand(void)
{
	/* setup bus-width */
	if (wr_mch_nand_data.bus_width_16)
		wr_mch_nand_smc_config.mode |= AT91_SMC_DBW_16;
	else
		wr_mch_nand_smc_config.mode |= AT91_SMC_DBW_8;

	/* configure chip-select 3 (NAND) */
	sam9_smc_configure(3, &wr_mch_nand_smc_config);

	at91_add_device_nand(&wr_mch_nand_data);
}

/*
 * CPU-FPGA interface
 */
static void __init wr_mch_cpu_fpga_init(void)
{
	/* select B peripheral mode for PA16..31 (EBI1_DATA16..31) */
	at91_set_B_periph(AT91_PIN_PA16, 0);
	at91_set_B_periph(AT91_PIN_PA17, 0);
	at91_set_B_periph(AT91_PIN_PA18, 0);
	at91_set_B_periph(AT91_PIN_PA19, 0);
	at91_set_B_periph(AT91_PIN_PA20, 0);
	at91_set_B_periph(AT91_PIN_PA21, 0);
	at91_set_B_periph(AT91_PIN_PA22, 0);
	at91_set_B_periph(AT91_PIN_PA23, 0);
	at91_set_B_periph(AT91_PIN_PA24, 0);
	at91_set_B_periph(AT91_PIN_PA25, 0);
	at91_set_B_periph(AT91_PIN_PA26, 0);
	at91_set_B_periph(AT91_PIN_PA27, 0);
	at91_set_B_periph(AT91_PIN_PA28, 0);
	at91_set_B_periph(AT91_PIN_PA29, 0);
	at91_set_B_periph(AT91_PIN_PA30, 0);
	at91_set_B_periph(AT91_PIN_PA31, 0);
	/* enable NWAIT */
	at91_set_B_periph(AT91_PIN_PE20, 0);

	/* enable main FPGA reset */
	at91_set_GPIO_periph(AT91_PIN_PD4, 0);
	/* and set it high */
	at91_set_gpio_output(AT91_PIN_PD4, 1);

	/* configure the Static Memory Controller to access the FPGA */
	at91_sys_write(AT91_SMC1_SETUP(0),
		AT91_SMC_NWESETUP_(4) |
		AT91_SMC_NCS_WRSETUP_(2) |
		AT91_SMC_NRDSETUP_(4) |
		AT91_SMC_NCS_RDSETUP_(2));
	at91_sys_write(AT91_SMC1_PULSE(0),
		AT91_SMC_NWEPULSE_(30) |
		AT91_SMC_NCS_WRPULSE_(34) |
		AT91_SMC_NRDPULSE_(30) |
		AT91_SMC_NCS_RDPULSE_(34));
	at91_sys_write(AT91_SMC1_CYCLE(0),
		AT91_SMC_NWECYCLE_(40) |
		AT91_SMC_NRDCYCLE_(40));
	at91_sys_write(AT91_SMC1_MODE(0),
		AT91_SMC_DBW_32 |
		AT91_SMC_TDF_(0) |
		AT91_SMC_READMODE |
		AT91_SMC_WRITEMODE);
}


static void __init wr_mch_board_init(void)
{
	wr_mch_cpu_fpga_init();

	at91_add_device_serial();

	/* select spi0 clock */
	at91_set_gpio_output(AT91_PIN_PE20, 1);

	at91_add_device_spi(wr_mch_spi_devices, ARRAY_SIZE(wr_mch_spi_devices));
	at91_add_device_eth(&wr_mch_macb_data);
	/*
	 * temporarily commented out: NAND on our board is broken
	 * wr_mch_add_device_nand();
	 */
	at91_add_device_i2c(NULL, 0);

	/* initialise the NIC */
	wr_mch_add_device_nic();

	/* shutdown controller, wakeup button (5 msec low) */
	at91_sys_write(AT91_SHDW_MR, AT91_SHDW_CPTWK0_(10) |
		AT91_SHDW_WKMODE0_LOW | AT91_SHDW_RTTWKEN);
}

MACHINE_START(WHITERABBIT_MCH, "White Rabbit MCH")
	/* Maintainer: CERN BE/CO/HT */
	.phys_io	= AT91_BASE_SYS,
	.io_pg_offst	= (AT91_VA_BASE_SYS >> 18) & 0xfffc,
	.boot_params	= AT91_SDRAM_BASE + 0x100,
	.timer		= &at91sam926x_timer,
	.map_io		= wr_mch_map_io,
	.init_irq	= wr_mch_init_irq,
	.init_machine	= wr_mch_board_init,
MACHINE_END
