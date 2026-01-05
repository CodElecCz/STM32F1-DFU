#include "spi_sram.h"
#include <stddef.h>
#include <stdint.h>

#define SPI_SRAM_PORT        1  // GPIO port number (0=A, 1=B, 2=C, etc.)
#define SPI_SRAM_SCK_PIN     13 // SCK pin number (0..15)
#define SPI_SRAM_MOSI_PIN    15 // MOSI pin number (0..15)
#define SPI_SRAM_MISO_PIN    14 // MISO pin number (0..15)
#define SPI_SRAM_CS_PIN      12 // CS pin number (0..15)

// GPIO base address calculation for STM32F1
static inline volatile uint32_t *gpio_base(uint32_t port)
{
    return (volatile uint32_t *)(0x40010800UL + (port) * 0x400UL);
}
static inline volatile uint32_t *gpio_crl(uint32_t port) { return gpio_base(port) + 0; }
static inline volatile uint32_t *gpio_crh(uint32_t port) { return gpio_base(port) + 1; }
static inline volatile uint32_t *gpio_idr(uint32_t port) { return gpio_base(port) + 2; }
static inline volatile uint32_t *gpio_odr(uint32_t port) { return gpio_base(port) + 3; }
static inline volatile uint32_t *gpio_bsrr(uint32_t port) { return gpio_base(port) + 4; }

// Local helpers to set pin mode (mimic simple parts of main.c gpio helpers)
// mode values follow existing convention used in main.c (nibble per pin):
// 0x0 = input analog, 0x4/0x8 used in original, we'll use 0x0 for input,
// 0x2 = output push-pull 2MHz
static void gpio_set_mode_local(uint32_t port, uint16_t pin, uint8_t mode_nibble)
{
    volatile uint32_t *cr;
    uint32_t shift;
    if (pin < 8)
    {
        cr = gpio_crl(port);
        shift = (pin) << 2;
    }
    else
    {
        cr = gpio_crh(port);
        shift = (pin - 8) << 2;
    }
    uint32_t v = *cr;
    v &= ~(0xFUL << shift);
    v |= ((uint32_t)mode_nibble << shift);
    *cr = v;
}

static void gpio_set_output_local(uint32_t port, uint16_t pin) { gpio_set_mode_local(port, pin, 0x2); }
static void gpio_set_input_local(uint32_t port, uint16_t pin)  { gpio_set_mode_local(port, pin, 0x0); }

static inline void gpio_set_local(uint32_t port, uint16_t pin)
{
    *gpio_bsrr(port) = (1UL << pin);
}
static inline void gpio_clear_local(uint32_t port, uint16_t pin)
{
    *gpio_bsrr(port) = (1UL << (16 + pin));
}
static inline uint32_t gpio_read_local(uint32_t port, uint16_t pin)
{
    return ((*gpio_idr(port)) & (1UL << pin)) ? 1U : 0U;
}

// Small delay for bit-bang timing. Tunable as needed for target speed.
static void spi_sram_delay(void)
{
    // ~6-12 CPU cycles per iteration depending on optimisation; small loop
    for (volatile int i = 0; i < 10; ++i) __asm__("nop");
}

// Default weak CS hooks - these will be used if board code doesn't provide
// its own implementation. They operate the CS pin defined above.
void spi_sram_cs_assert(void) { gpio_clear_local(SPI_SRAM_PORT, SPI_SRAM_CS_PIN); }
void spi_sram_cs_deassert(void) { gpio_set_local(SPI_SRAM_PORT, SPI_SRAM_CS_PIN); }

// Ensure GPIO pins are configured. Weak so a board can override to use RAM
// initialisation or alternate configurations.
void spi_sram_hw_init(void) __attribute__((weak));
void spi_sram_hw_init(void)
{
    // Configure SCK, MOSI and CS as outputs; MISO as input.
    gpio_set_output_local(SPI_SRAM_PORT, SPI_SRAM_SCK_PIN);
    gpio_set_output_local(SPI_SRAM_PORT, SPI_SRAM_MOSI_PIN);
    gpio_set_input_local(SPI_SRAM_PORT, SPI_SRAM_MISO_PIN);
    gpio_set_output_local(SPI_SRAM_PORT, SPI_SRAM_CS_PIN);
    // Idle lines: SCK low, CS high
    gpio_clear_local(SPI_SRAM_PORT, SPI_SRAM_SCK_PIN);
    gpio_set_local(SPI_SRAM_PORT, SPI_SRAM_CS_PIN);
}

// Transfer a single byte MSB-first and read incoming bits (mode 0: CPOL=0, CPHA=0)
static uint8_t spi_sram_xfer_byte(uint8_t out)
{
    uint8_t in = 0;
    for (int b = 7; b >= 0; --b)
    {
        // Set MOSI according to bit
        if (out & (1U << b))
            gpio_set_local(SPI_SRAM_PORT, SPI_SRAM_MOSI_PIN);
        else
            gpio_clear_local(SPI_SRAM_PORT, SPI_SRAM_MOSI_PIN);

        spi_sram_delay();
        // Clock high
        gpio_set_local(SPI_SRAM_PORT, SPI_SRAM_SCK_PIN);
        spi_sram_delay();
        // Sample MISO
        in <<= 1;
        in |= (uint8_t)(gpio_read_local(SPI_SRAM_PORT, SPI_SRAM_MISO_PIN) ? 1 : 0);
        // Clock low
        gpio_clear_local(SPI_SRAM_PORT, SPI_SRAM_SCK_PIN);
    }
    return in;
}

int spi_sram_read(uint32_t addr, void *buf, uint32_t len)
{
    if (!buf || !len)
        return -1;

    // Init hardware pins (weak - override if needed)
    spi_sram_hw_init();

    // 23LCV read command (standard SPI SRAM)
    const uint8_t CMD_READ = 0x03;

    // Assert CS
    spi_sram_cs_assert();

    // Send READ opcode and 24-bit address (MSB first)
    (void)spi_sram_xfer_byte(CMD_READ);
    (void)spi_sram_xfer_byte((uint8_t)((addr >> 16) & 0xFF));
    (void)spi_sram_xfer_byte((uint8_t)((addr >> 8) & 0xFF));
    (void)spi_sram_xfer_byte((uint8_t)((addr >> 0) & 0xFF));

    // Read data bytes by clocking zeros
    uint8_t *dst = (uint8_t*)buf;
    for (uint32_t i = 0; i < len; ++i)
    {
        dst[i] = spi_sram_xfer_byte(0x00);
    }

    // Deassert CS
    spi_sram_cs_deassert();

    return 0;
}
