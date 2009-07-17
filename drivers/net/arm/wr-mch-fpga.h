#ifndef __WR_MCH_FPGA_H
#define __WR_MCH_FPGA_H

#include <linux/bitops.h>

#define WR_FPGA_BASE_GPIO		0x00000
#define WR_FPGA_BASE_SPIM		0x40000
#define WR_FPGA_BASE_GIGASPY_UP0	0x80000
#define WR_FPGA_BASE_GIGASPY_UP1	0xc0000

#define WR_FPGA_BASE_EP_UP1		0x100000
#define WR_FPGA_BASE_NIC		0x140000

#define WR_GPIO_REG_CODR		0x0
#define WR_GPIO_REG_SODR		0x4
#define WR_GPIO_REG_DDR			0x8
#define WR_GPIO_REG_PSR			0xc

#define WR_SPI_REG_SPITX		0x4
#define WR_SPI_REG_SPIRX		0x8
#define WR_SPI_REG_SPICTL		0x0

#define WR_SPICTL_ENABLE(x)		((x)?BIT(2):0)
#define WR_SPICTL_CSEN(x)		((x)?BIT(0):0)
#define WR_SPICTL_CSDIS(x)		((x)?BIT(1):0)
#define WR_SPICTL_BUSY(rval)		((rval)&BIT(3)?1:0)
#define WR_SPICTL_CLKDIV(value)		(((value)&0xff) << 8)

#define WR_GPIO_PIN_LED0_A		0
#define WR_GPIO_PIN_LED0_K		1
#define WR_GPIO_PIN_LED1_A		2
#define WR_GPIO_PIN_LED1_K		3
#define WR_GPIO_PIN_LED2_A		4
#define WR_GPIO_PIN_LED2_K		5
#define WR_GPIO_PIN_LED3_A		6
#define WR_GPIO_PIN_LED3_K		7
#define WR_GPIO_PIN_CLKB_RST		31

#define WR_GPIO_PIN_UP0_SFP_SDA		8
#define WR_GPIO_PIN_UP0_SFP_SCL		9
#define WR_GPIO_PIN_UP0_SFP_LOS		10
#define WR_GPIO_PIN_UP0_SFP_TX_FAULT	11
#define WR_GPIO_PIN_UP0_SFP_TX_DISABLE	12
#define WR_GPIO_PIN_UP0_SFP_DETECT	13
#define WR_GPIO_PIN_UP0_PRBSEN		14
#define WR_GPIO_PIN_UP0_SYNCEN		15
#define WR_GPIO_PIN_UP0_LOOPEN		16
#define WR_GPIO_PIN_UP0_ENABLE		17

#define WR_GPIO_PIN_UP1_SFP_SDA		18
#define WR_GPIO_PIN_UP1_SFP_SCL		19
#define WR_GPIO_PIN_UP1_SFP_LOS		20
#define WR_GPIO_PIN_UP1_SFP_TX_FAULT	21
#define WR_GPIO_PIN_UP1_SFP_TX_DISABLE	22
#define WR_GPIO_PIN_UP1_SFP_DETECT	23
#define WR_GPIO_PIN_UP1_PRBSEN		24
#define WR_GPIO_PIN_UP1_SYNCEN		25
#define WR_GPIO_PIN_UP1_LOOPEN		26
#define WR_GPIO_PIN_UP1_ENABLE		27


/* GigaSpy wishbone registers */
#define WR_GIGASPY_MEM_SIZE		8192

#define WR_GSPY_REG_GSCTL		0x0
#define WR_GSPY_REG_GSTRIGCTL		0x4
#define WR_GSPY_REG_GSSTAT		0x8
#define WR_GSPY_REG_GSNSAMPLES		0xc
#define WR_GSPY_REG_GSTRIGADDR		0x10

#define WR_GSPY_MEM_CH0			0x8000
#define WR_GSPY_MEM_CH1			0x10000

#define WR_GSPY_GSCTL_CH0_ENABLE(x)	((x)?BIT(0):(0))
#define WR_GSPY_GSCTL_CH1_ENABLE(x)	((x)?BIT(1):(0))
#define WR_GSPY_GSCTL_SLAVE0_ENABLE(x)	((x)?BIT(2):(0))
#define WR_GSPY_GSCTL_SLAVE1_ENABLE(x)	((x)?BIT(3):(0))
#define WR_GSPY_GSCTL_LOAD_TRIG0(x)	((x)?BIT(4):(0))
#define WR_GSPY_GSCTL_LOAD_TRIG1(x)	((x)?BIT(5):(0))
#define WR_GSPY_GSCTL_RESET_TRIG0(x)	((x)?BIT(6):(0))
#define WR_GSPY_GSCTL_RESET_TRIG1(x)	((x)?BIT(7):(0))

#define WR_GSPY_GSTRIGCTL_TRIG0_VAL(k,v) (((k)?BIT(8):0) | ((v) & 0xff))
#define WR_GSPY_GSTRIGCTL_TRIG1_VAL(k,v) (((k)?BIT(24):0) | (((v) & 0xff)<<16))

#define WR_GSPY_GSTRIGCTL_TRIG0_EN(x)	(((x)?BIT(15):0))
#define WR_GSPY_GSTRIGCTL_TRIG1_EN(x)	(((x)?BIT(31):0))

#define WR_GSPY_GSNSAMPLES(x)		((x)&0x1fff)

#define WR_GSPY_GSSTAT_TRIG_DONE0(reg)	((reg)&BIT(0)?1:0)
#define WR_GSPY_GSSTAT_TRIG_DONE1(reg)	((reg)&BIT(1)?1:0)

#define WR_GSPY_GSSTAT_TRIG_SLAVE0(reg)	((reg)&BIT(2)?1:0)
#define WR_GSPY_GSSTAT_TRIG_SLAVE1(reg)	((reg)&BIT(3)?1:0)

#define WR_GSPY_GSTRIGADDR_CH0(reg)	((reg)&0x1fff)
#define WR_GSPY_GSTRIGADDR_CH1(reg)	(((reg)>>16)&0x1fff)

#define WR_CLKB_REG_IDCODE		0
#define WR_CLKB_REG_EXTPLLIO		1
#define WR_CLKB_REG_MUXIO	 	2
#define WR_CLKB_REG_DMTD_DAC		3
#define WR_CLKB_REG_REF_DAC		4
#define WR_CLKB_REG_DMTD_FREQ		8
#define WR_CLKB_REG_REF_FREQ		9
#define WR_CLKB_REG_EXT_FREQ		10
#define WR_CLKB_REG_UP0_FREQ		11
#define WR_CLKB_REG_UP1_FREQ		12

#define WR_CLKB_MUXIO_VCSO_SEL(x)	((x)?BIT(1):0)

#define WR_EP_REG_EPCTL			0x0
#define WR_EP_REG_EPSTA			0x4
#define WR_EP_REG_EPPHYIO		0x8
#define WR_EP_REG_EPTSVAL		0xc
#define WR_EP_REG_EPLCRCTL		0x10
#define WR_EP_REG_EPTXLCR		0x14
#define WR_EP_REG_EPRXLCR		0x18
#define WR_EP_REG_EPTSTAG		0x1c
#define WR_EP_REG_EPDMTDR		0x20
#define WR_EP_REG_EPRXCNTR		0x24

#define WR_EP_REG_EPDBGREG0		(0xc*4)

#define WR_EPCTL_FRAMER_LOOPBACK_ENABLE	BIT(13)
#define WR_EPCTL_RX_EMBED_OOB		BIT(11)

#define WR_EPCTL_RX_PCS_ENABLE		BIT(10)
#define WR_EPCTL_TX_PCS_ENABLE		BIT(8)

#define WR_EPSTA_TSFIFO_EMPTY(x)	(x&BIT(0))

#define WR_EPPHYIO_LOOPEN(x)		((x)?BIT(0):0)
#define WR_EPPHYIO_PRBSEN(x)		((x)?BIT(1):0)
#define WR_EPPHYIO_ENABLE(x)		((x)?BIT(2):0)
#define WR_EPPHYIO_SYNCEN(x)		((x)?BIT(3):0)
#define WR_EPPHYIO_TXCLKSEL(x)		((x)?BIT(4):0)

#endif /* __WR_MCH_FPGA_H */
