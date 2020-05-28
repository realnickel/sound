/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _LINUX_GPIO_REGMAP_H
#define _LINUX_GPIO_REGMAP_H

struct gpio_regmap;

#define GPIO_REGMAP_ADDR_ZERO ((unsigned long)(-1))
#define GPIO_REGMAP_ADDR(addr) ((addr) ? : GPIO_REGMAP_ADDR_ZERO)

/**
 * struct gpio_regmap_config - Description of a generic regmap gpio_chip.
 *
 * @parent:		The parent device
 * @regmap:		The regmap used to access the registers
 *			given, the name of the device is used
 * @label:		(Optional) Descriptive name for GPIO controller.
 *			If not given, the name of the device is used.
 * @names:		(Optional) Array of names for gpios
 * @ngpio:		Number of GPIOs
 * @reg_dat_base:	(Optional) (in) register base address
 * @reg_set_base:	(Optional) set register base address
 * @reg_clr_base:	(Optional) clear register base address
 * @reg_dir_in_base:	(Optional) in setting register base address
 * @reg_dir_out_base:	(Optional) out setting register base address
 * @reg_stride:		(Optional) May be set if the registers (of the
 *			same type, dat, set, etc) are not consecutive.
 * @ngpio_per_reg:	Number of GPIOs per register
 * @irq_domain:		(Optional) IRQ domain if the controller is
 *			interrupt-capable
 * @reg_mask_xlate:     (Optional) Translates base address and GPIO
 *			offset to a register/bitmask pair. If not
 *			given the default gpio_regmap_simple_xlate()
 *			is used.
 *
 * The reg_mask_xlate translates a given base address and GPIO offset to
 * register and mask pair. The base address is one of the given reg_*_base.
 *
 * All base addresses may have the special value GPIO_REGMAP_ADDR_ZERO
 * which forces the address to the value 0.
 */
struct gpio_regmap_config {
	struct device *parent;
	struct regmap *regmap;

	const char *label;
	const char *const *names;
	int ngpio;

	unsigned int reg_dat_base;
	unsigned int reg_set_base;
	unsigned int reg_clr_base;
	unsigned int reg_dir_in_base;
	unsigned int reg_dir_out_base;
	int reg_stride;
	int ngpio_per_reg;
	struct irq_domain *irq_domain;

	int (*reg_mask_xlate)(struct gpio_regmap *gpio, unsigned int base,
			      unsigned int offset, unsigned int *reg,
			      unsigned int *mask);
};

struct gpio_regmap *gpio_regmap_register(const struct gpio_regmap_config *config);
void gpio_regmap_unregister(struct gpio_regmap *gpio);
struct gpio_regmap *devm_gpio_regmap_register(struct device *dev,
					      const struct gpio_regmap_config *config);
void gpio_regmap_set_drvdata(struct gpio_regmap *gpio, void *data);
void *gpio_regmap_get_drvdata(struct gpio_regmap *gpio);

#endif /* _LINUX_GPIO_REGMAP_H */
