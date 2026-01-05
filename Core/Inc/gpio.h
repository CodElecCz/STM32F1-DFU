#ifndef __GPIO_H
#define __GPIO_H

#include <stdint.h>

// GPIO port identifiers
#define GPIOA 0
#define GPIOB 1
#define GPIOC 2
#define GPIOD 3
#define GPIOE 4
#define GPIOF 5

// STM32F1 GPIO register access (port number -> base)
#define GPIO_CRL(x)  *((volatile uint32_t*)((x)*0x400 +  0 + 0x40010800U))
#define GPIO_CRH(x)  *((volatile uint32_t*)((x)*0x400 +  4 + 0x40010800U))
#define GPIO_IDR(x)  *((volatile uint32_t*)((x)*0x400 +  8 + 0x40010800U))
#define GPIO_BSRR(x) *((volatile uint32_t*)((x)*0x400 + 16 + 0x40010800U))

// RCC APB2 peripheral clock enable register
#define RCC_APB2ENR  (*(volatile uint32_t*)0x40021018U)

// Enable GPIO port clock (port: 0=A .. 5=F)
static inline void rcc_gpio_enable(uint32_t gpion)
{
    RCC_APB2ENR |= (1 << (gpion + 2));
}

// Mode setter (nibble per pin as in original project)
void gpio_set_mode(uint32_t gpiodev, uint16_t gpion, uint8_t mode);

// Convenience macros mirroring original code
#define gpio_set_output(a,b)    gpio_set_mode(a,b,0x2)
#define gpio_set_input(a,b)     gpio_set_mode(a,b,0x0)
#define gpio_set_input_pp(a,b)  gpio_set_mode(a,b,0x8)

// Basic GPIO operations
void gpio_clear(uint32_t gpiodev, uint16_t gpion);
void gpio_set(uint32_t gpiodev, uint16_t gpion);
uint32_t gpio_read(uint32_t gpiodev, uint16_t gpion);

#endif // __GPIO_H
