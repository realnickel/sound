/*
 * GPIO controller driver for Intel Cherryview/Braswell.
 *
 * Copyright (C) 2014, Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/interrupt.h>
#include <linux/gpio/driver.h>
#include <linux/seq_file.h>
#include <linux/ioport.h>
#include <linux/acpi.h>
#include <linux/platform_device.h>

#define FAMILY0_PAD_REGS_OFF	0x4400
#define FAMILY_PAD_REGS_SIZE	0x400
#define MAX_FAMILY_PAD_GPIO_NO	15
#define GPIO_REGS_SIZE		8

#define CV_PADCTRL0_REG		0x000
#define CV_PADCTRL1_REG		0x004
#define CV_INT_STAT_REG		0x300
#define CV_INT_MASK_REG		0x380

#define CV_GPIO_RX_STAT		BIT(0)
#define CV_GPIO_TX_STAT		BIT(1)
#define CV_GPIO_EN		BIT(15)

#define CV_CFG_LOCK_MASK	BIT(31)
#define CV_INT_CFG_MASK		(BIT(0) | BIT(1) | BIT(2))
#define CV_PAD_MODE_MASK	(0xf << 16)

#define CV_GPIO_CFG_MASK	(BIT(8) | BIT(9) | BIT(10))
#define CV_GPIO_TX_EN		(1 << 8)
#define CV_GPIO_RX_EN		(2 << 8)

#define CV_INV_RX_DATA		BIT(6)

#define CV_INT_SEL_MASK		(0xf << 28)

enum {
	CV_INTR_DISABLE,
	CV_TRIG_EDGE_FALLING,
	CV_TRIG_EDGE_RISING,
	CV_TRIG_EDGE_BOTH,
	CV_TRIG_LEVEL,
};

struct chv_gpio_bank {
	const char *name;
	const char *uid;
	const char * const *pads;
	size_t npads;
};

struct chv_gpio {
	struct gpio_chip chip;
	spinlock_t lock;
	void __iomem *reg_base;
	int intr_lines[16];
	const struct chv_gpio_bank *bank;
};

static const char * const north_pads[] = {
	"GPIO_DFX_0",
	"GPIO_DFX_3",
	"GPIO_DFX_7",
	"GPIO_DFX_1",
	"GPIO_DFX_5",
	"GPIO_DFX_4",
	"GPIO_DFX_8",
	"GPIO_DFX_2",
	"GPIO_DFX_6",

	[9 ... 14] = 0,

	"GPIO_SUS0",
	"SEC_GPIO_SUS10",
	"GPIO_SUS3",
	"GPIO_SUS7",
	"GPIO_SUS1",
	"GPIO_SUS5",
	"SEC_GPIO_SUS11",
	"GPIO_SUS4",
	"SEC_GPIO_SUS8",
	"GPIO_SUS2",
	"GPIO_SUS6",
	"CX_PREQ_B",
	"SEC_GPIO_SUS9",

	[28 ... 29] = 0,

	"TRST_B",
	"TCK",
	"PROCHOT_B",
	"SVIDO_DATA",
	"TMS",
	"CX_PRDY_B_2",
	"TDO_2",
	"CX_PRDY_B",
	"SVIDO_ALERT_B",
	"TDO",
	"SVIDO_CLK",
	"TDI",

	[42 ... 44] = 0,

	"GP_CAMERASB_05",
	"GP_CAMERASB_02",
	"GP_CAMERASB_08",
	"GP_CAMERASB_00",
	"GP_CAMERASB_06",
	"GP_CAMERASB_10",
	"GP_CAMERASB_03",
	"GP_CAMERASB_09",
	"GP_CAMERASB_01",
	"GP_CAMERASB_07",
	"GP_CAMERASB_11",
	"GP_CAMERASB_04",

	[57 ... 59] = 0,

	"PANEL0_BKLTEN",
	"HV_DDI0_HPD",
	"HV_DDI2_DDC_SDA",
	"PANEL1_BKLTCTL",
	"HV_DDI1_HPD",
	"PANEL0_BKLTCTL",
	"HV_DDI0_DDC_SDA",
	"HV_DDI2_DDC_SCL",
	"HV_DDI2_HPD",
	"PANEL1_VDDEN",
	"PANEL1_BKLTEN",
	"HV_DDI0_DDC_SCL",
	"PANEL0_VDDEN",
};

static const char * const southeast_pads[] = {
	"MF_PLT_CLK0",
	"PWM1",
	"MF_PLT_CLK1",
	"MF_PLT_CLK4",
	"MF_PLT_CLK3",
	"PWM0",
	"MF_PLT_CLK5",
	"MF_PLT_CLK2",

	[9 ... 14] = 0,

	"SDMMC2_D3_CD_B",
	"SDMMC1_CLK",
	"SDMMC1_D0",
	"SDMMC2_D1",
	"SDMMC2_CLK",
	"SDMMC1_D2",
	"SDMMC2_D2",
	"SDMMC2_CMD",
	"SDMMC1_CMD",
	"SDMMC1_D1",
	"SDMMC2_D0",
	"SDMMC1_D3_CD_B",

	[27 ... 29] = 0,

	"SDMMC3_D1",
	"SDMMC3_CLK",
	"SDMMC3_D3",
	"SDMMC3_D2",
	"SDMMC3_CMD",
	"SDMMC3_D0",

	[36 ... 44] = 0,

	"MF_LPC_AD2",
	"LPC_CLKRUNB",
	"MF_LPC_AD0",
	"LPC_FRAMEB",
	"MF_LPC_CLKOUT1",
	"MF_LPC_AD3",
	"MF_LPC_CLKOUT0",
	"MF_LPC_AD1",

	[53 ... 59] = 0,

	"SPI1_MISO",
	"SPI1_CSO_B",
	"SPI1_CLK",
	"MMC1_D6",
	"SPI1_MOSI",
	"MMC1_D5",
	"SPI1_CS1_B",
	"MMC1_D4_SD_WE",
	"MMC1_D7",
	"MMC1_RCLK",

	[70 ... 74] = 0,

	"USB_OC1_B",
	"PMU_RESETBUTTON_B",
	"GPIO_ALERT",
	"SDMMC3_PWR_EN_B",
	"ILB_SERIRQ",
	"USB_OC0_B",
	"SDMMC3_CD_B",
	"SPKR",
	"SUSPWRDNACK",
	"SPARE_PIN",
	"SDMMC3_1P8_EN",
};

static const char * const east_pads[] = {
	"PMU_SLP_S3_B",
	"PMU_BATLOW_B",
	"SUS_STAT_B",
	"PMU_SLP_S0IX_B",
	"PMU_AC_PRESENT",
	"PMU_PLTRST_B",
	"PMU_SUSCLK",
	"PMU_SLP_LAN_B",
	"PMU_PWRBTN_B",
	"PMU_SLP_S4_B",
	"PMU_WAKE_B",
	"PMU_WAKE_LAN_B",

	[12 ... 14] = 0,

	"MF_ISH_GPIO_3",
	"MF_ISH_GPIO_7",
	"MF_ISH_I2C1_SCL",
	"MF_ISH_GPIO_1",
	"MF_ISH_GPIO_5",
	"MF_ISH_GPIO_9",
	"MF_ISH_GPIO_0",
	"MF_ISH_GPIO_4",
	"MF_ISH_GPIO_8",
	"MF_ISH_GPIO_2",
	"MF_ISH_GPIO_6",
	"MF_ISH_I2C1_SDA",
};

static const char * const southwest_pads[] = {
	"FST_SPI_D2",
	"FST_SPI_D0",
	"FST_SPI_CLK",
	"FST_SPI_D3",
	"FST_SPI_CS1_B",
	"FST_SPI_D1",
	"FST_SPI_CS0_B",
	"FST_SPI_CS2_B",

	[8 ... 14] = 0,

	"UART1_RTS_B",
	"UART1_RXD",
	"UART2_RXD",
	"UART1_CTS_B",
	"UART2_RTS_B",
	"UART1_TXD",
	"UART2_TXD",
	"UART2_CTS_B",

	[23 ... 29] = 0,

	"MF_HDA_CLK",
	"MF_HDA_RSTB",
	"MF_HDA_SDIO",
	"MF_HDA_SDO",
	"MF_HDA_DOCKRSTB",
	"MF_HDA_SYNC",
	"MF_HDA_SDI1",
	"MF_HDA_DOCKENB",

	[38 ... 44] = 0,

	"I2C5_SDA",
	"I2C4_SDA",
	"I2C6_SDA",
	"I2C5_SCL",
	"I2C_NFC_SDA",
	"I2C4_SCL",
	"I2C6_SCL",
	"I2C_NFC_SCL",

	[53 ... 59] = 0,

	"I2C1_SDA",
	"I2C0_SDA",
	"I2C2_SDA",
	"I2C1_SCL",
	"I2C3_SDA",
	"I2C0_SCL",
	"I2C2_SCL",
	"I2C3_SCL",

	[68 ... 74] = 0,

	"SATA_GP0",
	"SATA_GP1",
	"SATA_LEDN",
	"SATA_GP2",
	"MF_SMB_ALERTB",
	"SATA_GP3",
	"MF_SMB_CLK",
	"MF_SMB_DATA",

	[83 ... 89] = 0,

	"PCIE_CLKREQ0B",
	"PCIE_CLKREQ1B",
	"GP_SSP_2_CLK",
	"PCIE_CLKREQ2B",
	"GP_SSP_2_RXD",
	"PCIE_CLKREQ3B",
	"GP_SSP_2_FS",
	"GP_SSP_2_TXD",
};

static const struct chv_gpio_bank chv_banks[] = {
	{
		.name = "SW",
		.uid = "1",
		.pads = southwest_pads,
		.npads = ARRAY_SIZE(southwest_pads),
	},
	{
		.name = "N",
		.uid = "2",
		.pads = north_pads,
		.npads = ARRAY_SIZE(north_pads),
	},
	{
		.name = "E",
		.uid = "3",
		.pads = east_pads,
		.npads = ARRAY_SIZE(east_pads),
	},
	{
		.name = "SE",
		.uid = "4",
		.pads = southeast_pads,
		.npads = ARRAY_SIZE(southeast_pads),
	},
};

#define to_chv_gpio(c)	container_of(c, struct chv_gpio, chip)

static void __iomem *chv_gpio_reg(struct chv_gpio *cg, unsigned offset, int reg)
{
	u32 reg_offset;

	if (reg == CV_INT_STAT_REG || reg == CV_INT_MASK_REG) {
		reg_offset = 0;
	} else {
		reg_offset = FAMILY0_PAD_REGS_OFF +
		      FAMILY_PAD_REGS_SIZE * (offset / MAX_FAMILY_PAD_GPIO_NO) +
		      GPIO_REGS_SIZE * (offset % MAX_FAMILY_PAD_GPIO_NO);
	}

	return cg->reg_base + reg_offset + reg;
}

static inline void chv_writel(u32 value, void __iomem *reg)
{
	writel(value, reg);
	/* simple readback to confirm the bus transferring done */
	readl(reg);
}

/* When Pad Cfg is locked, driver can only change GPIOTXState or GPIORXState */
static inline bool chv_gpio_pad_locked(struct chv_gpio *cg, unsigned offset)
{
	void __iomem *reg;

	reg = chv_gpio_reg(cg, offset, CV_PADCTRL1_REG);
	return readl(reg) & CV_CFG_LOCK_MASK;
}

static int chv_gpio_request(struct gpio_chip *chip, unsigned offset)
{
	struct chv_gpio *cg = to_chv_gpio(chip);
	void __iomem *reg;
	u32 value;

	if (!cg->bank->pads[offset])
		return -EINVAL;

	if (chv_gpio_pad_locked(cg, offset))
		return 0;

	/* Disable interrupt generation */
	reg = chv_gpio_reg(cg, offset, CV_PADCTRL1_REG);
	value = readl(reg);
	value &= ~(CV_INT_CFG_MASK | CV_INV_RX_DATA);
	chv_writel(value, reg);

	/* Switch to a GPIO mode */
	reg = chv_gpio_reg(cg, offset, CV_PADCTRL0_REG);
	value = readl(reg) | CV_GPIO_EN;
	chv_writel(value, reg);

	return 0;
}

static void chv_gpio_free(struct gpio_chip *chip, unsigned offset)
{
	struct chv_gpio *cg = to_chv_gpio(chip);
	void __iomem *reg;
	u32 value;

	if (chv_gpio_pad_locked(cg, offset))
		return;

	reg = chv_gpio_reg(cg, offset, CV_PADCTRL0_REG);
	value = readl(reg) & ~CV_GPIO_EN;
	chv_writel(value, reg);
}

static void chv_update_irq_type(struct chv_gpio *cg, unsigned type,
				void __iomem *reg)
{
	u32 value;

	value = readl(reg);
	value &= ~CV_INT_CFG_MASK;
	value &= ~CV_INV_RX_DATA;

	if (type & IRQ_TYPE_EDGE_BOTH) {
		if ((type & IRQ_TYPE_EDGE_BOTH) == IRQ_TYPE_EDGE_BOTH)
			value |= CV_TRIG_EDGE_BOTH;
		else if (type & IRQ_TYPE_EDGE_RISING)
			value |= CV_TRIG_EDGE_RISING;
		else if (type & IRQ_TYPE_EDGE_FALLING)
			value |= CV_TRIG_EDGE_FALLING;
	} else if (type & IRQ_TYPE_LEVEL_MASK) {
			value |= CV_TRIG_LEVEL;
		if (type & IRQ_TYPE_LEVEL_LOW)
			value |= CV_INV_RX_DATA;
	}

	chv_writel(value, reg);
}

/* BIOS programs IntSel bits for shared interrupt. GPIO driver follows it. */
static void pad_intr_line_save(struct chv_gpio *cg, unsigned offset)
{
	void __iomem *reg = chv_gpio_reg(cg, offset, CV_PADCTRL0_REG);
	u32 value, intr_line;

	value = readl(reg);
	intr_line = (value & CV_INT_SEL_MASK) >> 28;
	cg->intr_lines[intr_line] = offset;
}

static int chv_irq_type(struct irq_data *d, unsigned type)
{
	struct chv_gpio *cg = irq_data_get_irq_chip_data(d);
	u32 offset = irqd_to_hwirq(d);
	void __iomem *reg;
	unsigned long flags;

	spin_lock_irqsave(&cg->lock, flags);

	/*
	 * Pins which can be used as shared interrupt are configured in
	 * BIOS. Driver trusts BIOS configurations and assigns different
	 * handler according to the irq type.
	 *
	 * Driver needs to save the mapping between each pin and
	 * its interrupt line.
	 * 1. If the pin cfg is locked in BIOS:
	 *	Trust BIOS has programmed IntWakeCfg bits correctly,
	 *	driver just needs to save the mapping.
	 * 2. If the pin cfg is not locked in BIOS:
	 *	Driver programs the IntWakeCfg bits and save the mapping.
	 */
	if (!chv_gpio_pad_locked(cg, offset)) {
		reg = chv_gpio_reg(cg, offset, CV_PADCTRL1_REG);
		chv_update_irq_type(cg, type, reg);
	}

	pad_intr_line_save(cg, offset);

	if (type & IRQ_TYPE_EDGE_BOTH)
		__irq_set_handler_locked(d->irq, handle_edge_irq);
	else if (type & IRQ_TYPE_LEVEL_MASK)
		__irq_set_handler_locked(d->irq, handle_level_irq);

	spin_unlock_irqrestore(&cg->lock, flags);

	return 0;
}

static int chv_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct chv_gpio *cg = to_chv_gpio(chip);
	unsigned long flags;
	void __iomem *reg;
	u32 value;
	int ret;

	reg = chv_gpio_reg(cg, offset, CV_PADCTRL0_REG);

	spin_lock_irqsave(&cg->lock, flags);

	value = readl(reg);
	if (value & CV_GPIO_TX_EN)
		ret = !!(value & CV_GPIO_TX_STAT);
	else
		ret = !!(value & CV_GPIO_RX_STAT);

	spin_unlock_irqrestore(&cg->lock, flags);

	return ret;
}

static void chv_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct chv_gpio *cg = to_chv_gpio(chip);
	void __iomem *reg;
	unsigned long flags;
	u32 old_val;

	reg = chv_gpio_reg(cg, offset, CV_PADCTRL0_REG);

	spin_lock_irqsave(&cg->lock, flags);

	old_val = readl(reg);

	if (value)
		chv_writel(old_val | CV_GPIO_TX_STAT, reg);
	else
		chv_writel(old_val & ~CV_GPIO_TX_STAT, reg);

	spin_unlock_irqrestore(&cg->lock, flags);
}

static int chv_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	struct chv_gpio *cg = to_chv_gpio(chip);
	unsigned long flags;
	void __iomem *reg;
	u32 value;

	if (chv_gpio_pad_locked(cg, offset))
		return 0;

	reg = chv_gpio_reg(cg, offset, CV_PADCTRL0_REG);

	spin_lock_irqsave(&cg->lock, flags);

	value = readl(reg) & ~CV_GPIO_CFG_MASK;
	/* Disable TX and Enable RX */
	value |= CV_GPIO_RX_EN;
	chv_writel(value, reg);

	spin_unlock_irqrestore(&cg->lock, flags);

	return 0;
}

static int chv_gpio_direction_output(struct gpio_chip *chip, unsigned offset,
				     int value)
{
	struct chv_gpio *cg = to_chv_gpio(chip);
	unsigned long flags;
	void __iomem *reg;
	u32 reg_val;

	if (chv_gpio_pad_locked(cg, offset))
		return 0;

	reg = chv_gpio_reg(cg, offset, CV_PADCTRL0_REG);

	spin_lock_irqsave(&cg->lock, flags);
	reg_val = readl(reg) & ~CV_GPIO_CFG_MASK;
	reg_val |= CV_GPIO_TX_EN;

	/* Control TX State */
	if (value)
		reg_val |= CV_GPIO_TX_STAT;
	else
		reg_val &= ~CV_GPIO_TX_STAT;

	chv_writel(reg_val, reg);

	spin_unlock_irqrestore(&cg->lock, flags);
	return 0;
}

static void chv_gpio_dbg_show(struct seq_file *s, struct gpio_chip *chip)
{
	struct chv_gpio *cg = to_chv_gpio(chip);
	u32 ctrl0, ctrl1, offs;
	unsigned long flags;
	void __iomem *reg;
	int i;

	spin_lock_irqsave(&cg->lock, flags);

	for (i = 0; i < cg->chip.ngpio; i++) {
		const char *intcfg;
		const char *label;
		const char *value;
		const char *dir;
		char pin[8];

		if (!cg->bank->pads[i])
			continue;

		offs = FAMILY0_PAD_REGS_OFF +
		      FAMILY_PAD_REGS_SIZE * (i / MAX_FAMILY_PAD_GPIO_NO) +
		      GPIO_REGS_SIZE * (i % MAX_FAMILY_PAD_GPIO_NO);

		ctrl0 = readl(chv_gpio_reg(cg, i, CV_PADCTRL0_REG));
		ctrl1 = readl(chv_gpio_reg(cg, i, CV_PADCTRL1_REG));

		snprintf(pin, sizeof(pin), "%s%02d", cg->bank->name, i);

		switch (ctrl1 & CV_INT_CFG_MASK) {
		case CV_INTR_DISABLE:
			intcfg = "disabled";
			break;

		case CV_TRIG_EDGE_FALLING:
			intcfg = "falling";
			break;

		case CV_TRIG_EDGE_RISING:
			intcfg = "rising";
			break;

		case CV_TRIG_EDGE_BOTH:
			intcfg = "both";
			break;

		case CV_TRIG_LEVEL:
			if (ctrl1 & CV_INV_RX_DATA)
				intcfg = "low";
			else
				intcfg = "high";
			break;

		default:
			intcfg = "unknown";
			break;
		}

		switch ((ctrl0 & CV_GPIO_CFG_MASK) >> 8) {
		case 0:
			dir = "in out";
			break;
		case 1:
			dir = "   out";
			break;
		case 2:
			dir = "in";
			break;
		case 3:
			dir = "HiZ";
			break;
		default:
			dir = "unknown";
			break;
		}

		if (ctrl0 & CV_GPIO_TX_EN)
			value = ctrl0 & CV_GPIO_TX_STAT ? "high" : "low";
		else
			value = ctrl0 & CV_GPIO_RX_STAT ? "high" : "low";

		seq_printf(s,
			   "%c%-4s %-17s %-8s %-4s 0x%03x %d %-8s %02d 0x%08x 0x%08x",
			   chv_gpio_pad_locked(cg, i) ? '*' : ' ',
			   pin, cg->bank->pads[i], dir, value, offs,
			   (ctrl0 & CV_PAD_MODE_MASK) >> 16, intcfg,
			   (ctrl0 & CV_INT_SEL_MASK) >> 28, ctrl0, ctrl1);

		label = gpiochip_is_requested(&cg->chip, i);
		if (label)
			seq_printf(s, " %s\n", label);
		else
			seq_puts(s, "\n");
	}

	reg = chv_gpio_reg(cg, 0, CV_INT_STAT_REG);
	seq_printf(s, "CV_INT_STAT_REG: 0x%08x\n", readl(reg));

	reg = chv_gpio_reg(cg, 0, CV_INT_MASK_REG);
	seq_printf(s, "CV_INT_MASK_REG: 0x%08x\n", readl(reg));

	for (i = 0; i < ARRAY_SIZE(cg->intr_lines); i++) {
		if (cg->intr_lines[i] >= 0)
			seq_printf(s, "intline: %d, offset: %d\n", i,
				   cg->intr_lines[i]);
	}

	seq_puts(s, "\n");

	spin_unlock_irqrestore(&cg->lock, flags);
}

static void chv_irq_unmask(struct irq_data *d)
{
	struct chv_gpio *cg = irq_data_get_irq_chip_data(d);
	u32 offset = irqd_to_hwirq(d);
	u32 value, intr_line;
	unsigned long flags;
	void __iomem *reg;

	spin_lock_irqsave(&cg->lock, flags);

	reg = chv_gpio_reg(cg, offset, CV_PADCTRL0_REG);
	intr_line = (readl(reg) & CV_INT_SEL_MASK) >> 28;

	reg = chv_gpio_reg(cg, 0, CV_INT_MASK_REG);
	value = readl(reg);
	value |= (1 << intr_line);
	chv_writel(value, reg);

	spin_unlock_irqrestore(&cg->lock, flags);
}

static void chv_irq_mask(struct irq_data *d)
{
	struct chv_gpio *cg = irq_data_get_irq_chip_data(d);
	u32 offset = irqd_to_hwirq(d);
	u32 value, intr_line;
	unsigned long flags;
	void __iomem *reg;

	spin_lock_irqsave(&cg->lock, flags);

	reg = chv_gpio_reg(cg, offset, CV_PADCTRL0_REG);
	intr_line = (readl(reg) & CV_INT_SEL_MASK) >> 28;

	value = readl(reg);
	value &= ~(1 << intr_line);
	chv_writel(value, reg);

	spin_unlock_irqrestore(&cg->lock, flags);
}

static void chv_irq_ack(struct irq_data *d)
{
}

static void chv_irq_shutdown(struct irq_data *d)
{
	struct chv_gpio *cg = irq_data_get_irq_chip_data(d);
	u32 offset = irqd_to_hwirq(d);
	void __iomem *reg;
	unsigned long flags;

	reg = chv_gpio_reg(cg, offset, CV_PADCTRL1_REG);

	chv_irq_mask(d);

	if (!chv_gpio_pad_locked(cg, offset)) {
		spin_lock_irqsave(&cg->lock, flags);
		chv_update_irq_type(cg, IRQ_TYPE_NONE, reg);
		spin_unlock_irqrestore(&cg->lock, flags);
	}
}

static struct irq_chip chv_irqchip = {
	.name = "CHV-GPIO",
	.irq_mask = chv_irq_mask,
	.irq_unmask = chv_irq_unmask,
	.irq_set_type = chv_irq_type,
	.irq_ack = chv_irq_ack,
	.irq_shutdown = chv_irq_shutdown,
	.flags = IRQCHIP_SKIP_SET_WAKE,
};

static void chv_gpio_irq_handler(unsigned irq, struct irq_desc *desc)
{
	struct irq_data *data = irq_desc_get_irq_data(desc);
	struct chv_gpio *cg = to_chv_gpio(irq_desc_get_handler_data(desc));
	struct irq_chip *chip = irq_data_get_irq_chip(data);
	u32 intr_line, mask, offset;
	void __iomem *reg, *mask_reg;
	u32 pending;

	/* each GPIO controller has one INT_STAT reg */
	reg = chv_gpio_reg(cg, 0, CV_INT_STAT_REG);
	mask_reg = chv_gpio_reg(cg, 0, CV_INT_MASK_REG);
	while ((pending = (readl(reg) & readl(mask_reg) & 0xffff))) {
		unsigned irq;

		intr_line = __ffs(pending);
		mask = BIT(intr_line);
		chv_writel(mask, reg);
		offset = cg->intr_lines[intr_line];
		if (unlikely(offset < 0)) {
			dev_warn(cg->chip.dev, "unregistered shared irq\n");
			continue;
		}

		irq = irq_find_mapping(cg->chip.irqdomain, offset);
		generic_handle_irq(irq);
	}

	chip->irq_eoi(data);
}

static void chv_gpio_irq_init_hw(struct chv_gpio *cg)
{
	void __iomem *reg;

	/* Mask all interrupts */
	reg = chv_gpio_reg(cg, 0, CV_INT_MASK_REG);
	chv_writel(0, reg);

	reg = chv_gpio_reg(cg, 0, CV_INT_STAT_REG);
	chv_writel(0xffff, reg);
}

static const struct gpio_chip chv_gpio_chip = {
	.owner = THIS_MODULE,
	.request = chv_gpio_request,
	.free = chv_gpio_free,
	.direction_input = chv_gpio_direction_input,
	.direction_output = chv_gpio_direction_output,
	.get = chv_gpio_get,
	.set = chv_gpio_set,
	.dbg_show = chv_gpio_dbg_show,
	.base = -1,
};

static int chv_gpio_probe(struct platform_device *pdev)
{
	struct resource *mem_rc, *irq_rc;
	const struct chv_gpio_bank *bank;
	struct acpi_device *adev;
	struct chv_gpio *cg;
	int i, ret = 0;

	adev = ACPI_COMPANION(&pdev->dev);
	if (!adev)
		return -ENODEV;

	cg = devm_kzalloc(&pdev->dev, sizeof(*cg), GFP_KERNEL);
	if (!cg)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(chv_banks); i++) {
		bank = &chv_banks[i];
		if (!strcmp(adev->pnp.unique_id, bank->uid)) {
			cg->bank = bank;
			break;
		}
	}
	if (i == ARRAY_SIZE(chv_banks))
		return -ENODEV;

	mem_rc = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	cg->reg_base = devm_ioremap_resource(&pdev->dev, mem_rc);
	if (IS_ERR(cg->reg_base))
		return PTR_ERR(cg->reg_base);

	spin_lock_init(&cg->lock);
	cg->chip = chv_gpio_chip;
	cg->chip.ngpio = cg->bank->npads;
	cg->chip.label = dev_name(&pdev->dev);
	cg->chip.dev = &pdev->dev;

	/* Initialize interrupt lines array with negative value */
	for (i = 0; i < ARRAY_SIZE(cg->intr_lines); i++)
		cg->intr_lines[i] = -1;

	ret = gpiochip_add(&cg->chip);
	if (ret) {
		dev_err(&pdev->dev, "Failed adding GPIO chip\n");
		return ret;
	}

	irq_rc = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (irq_rc && irq_rc->start) {
		chv_gpio_irq_init_hw(cg);

		ret = gpiochip_irqchip_add(&cg->chip, &chv_irqchip, 0,
					   handle_simple_irq, IRQ_TYPE_NONE);
		if (ret) {
			dev_err(&pdev->dev, "Failed to add irqchip\n");
			gpiochip_remove(&cg->chip);
			return ret;
		}

		gpiochip_set_chained_irqchip(&cg->chip, &chv_irqchip,
					     (unsigned)irq_rc->start,
					     chv_gpio_irq_handler);
	}

	return 0;
}

static int chv_gpio_remove(struct platform_device *pdev)
{
	struct chv_gpio *cg = platform_get_drvdata(pdev);

	gpiochip_remove(&cg->chip);
	return 0;
}

static const struct acpi_device_id chv_gpio_acpi_match[] = {
	{ "INT33FF" },
	{ }
};
MODULE_DEVICE_TABLE(acpi, chv_gpio_acpi_match);

static struct platform_driver chv_gpio_driver = {
	.probe = chv_gpio_probe,
	.remove = chv_gpio_remove,
	.driver = {
		.name = "chv_gpio",
		.owner = THIS_MODULE,
		.acpi_match_table = ACPI_PTR(chv_gpio_acpi_match),
	},
};

static int __init chv_gpio_init(void)
{
	return platform_driver_register(&chv_gpio_driver);
}
subsys_initcall(chv_gpio_init);

static void __exit chv_gpio_exit(void)
{
	platform_driver_unregister(&chv_gpio_driver);
}
module_exit(chv_gpio_exit);

MODULE_DESCRIPTION("GPIO driver for Intel Cherryview/Braswell");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Ning Li <ning.li@intel.com>");
MODULE_AUTHOR("Alan Cox <alan@linux.intel.com>");
MODULE_AUTHOR("Mika Westerberg <mika.westerberg@linux.intel.com>");
MODULE_ALIAS("platform:chv_gpio");
