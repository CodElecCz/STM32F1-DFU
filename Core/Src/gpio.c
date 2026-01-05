#include "gpio.h"

inline void gpio_set_mode(uint32_t gpiodev, uint16_t gpion, uint8_t mode) {
	if (gpion < 8)
		GPIO_CRL(gpiodev) = (GPIO_CRL(gpiodev) & ~(0xf << ((gpion)<<2))) | (mode << ((gpion)<<2));
	else
		GPIO_CRH(gpiodev) = (GPIO_CRH(gpiodev) & ~(0xf << ((gpion-8)<<2))) | (mode << ((gpion-8)<<2));
}

void gpio_clear(uint32_t gpiodev, uint16_t gpion) {
	GPIO_BSRR(gpiodev) = (1 << (16 + gpion));
}

void gpio_set(uint32_t gpiodev, uint16_t gpion) {
	GPIO_BSRR(gpiodev) = (1 << (gpion));
}

uint32_t gpio_read(uint32_t gpiodev, uint16_t gpion) {
	return (GPIO_IDR(gpiodev) & (1 << (gpion))) ? 1U : 0U;
}
