/*
 * GPIO customization for x86.
 *
 * Based on the original code:
 *
 * Copyright (c) 2007-2008  MontaVista Software, Inc.
 * Author: Anton Vorontsov <avorontsov@ru.mvista.com>
 *
 * Copyright (c) 2014, Intel Corporation.
 * Author: Mika Westerberg <mika.westerberg@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_X86_GPIO_H
#define __ASM_X86_GPIO_H

#define ARCH_NR_GPIOS 512
#include <asm-generic/gpio.h>

#ifdef CONFIG_GPIOLIB
static inline int gpio_get_value(unsigned int gpio)
{
	return __gpio_get_value(gpio);
}

static inline void gpio_set_value(unsigned int gpio, int value)
{
	__gpio_set_value(gpio, value);
}

static inline int gpio_cansleep(unsigned int gpio)
{
	return __gpio_cansleep(gpio);
}

static inline int gpio_to_irq(unsigned int gpio)
{
	return __gpio_to_irq(gpio);
}

static inline int irq_to_gpio(unsigned int irq)
{
	return -EINVAL;
}
#endif /* CONFIG_GPIOLIB */

#endif /* __ASM_X86_GPIO_H */
