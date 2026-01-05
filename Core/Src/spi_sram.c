#include "spi_sram.h"
#include "gpio.h"
#include "board.h"
#include <stddef.h>
#include <stdint.h>

// Small delay for bit-bang timing. Tunable as needed for target speed.
static void spi_sram_delay(void)
{
    // ~6-12 CPU cycles per iteration depending on optimisation; small loop
    for (volatile int i = 0; i < 10; ++i) __asm__("nop");
}

// Default weak CS hooks - these will be used if board code doesn't provide
// its own implementation. They operate the CS pin defined above.
void spi_sram_cs_assert(void) { gpio_clear(SPI_SRAM_PORT, SPI_SRAM_CS_PIN); }
void spi_sram_cs_deassert(void) { gpio_set(SPI_SRAM_PORT, SPI_SRAM_CS_PIN); }

// Ensure GPIO pins are configured. Weak so a board can override to use RAM
// initialisation or alternate configurations.
void spi_sram_hw_init(void) __attribute__((weak));
void spi_sram_hw_init(void)
{
    // Enable GPIO clock for the SRAM port
    rcc_gpio_enable(SPI_SRAM_PORT);

    // Configure SCK, MOSI and CS as outputs; MISO as input.
    gpio_set_output(SPI_SRAM_PORT, SPI_SRAM_SCK_PIN);
    gpio_set_output(SPI_SRAM_PORT, SPI_SRAM_MOSI_PIN);
    gpio_set_input(SPI_SRAM_PORT, SPI_SRAM_MISO_PIN);
    gpio_set_output(SPI_SRAM_PORT, SPI_SRAM_CS_PIN);
    // Idle lines: SCK low, CS high
    gpio_clear(SPI_SRAM_PORT, SPI_SRAM_SCK_PIN);
    gpio_set(SPI_SRAM_PORT, SPI_SRAM_CS_PIN);
}

// Transfer a single byte MSB-first and read incoming bits (mode 0: CPOL=0, CPHA=0)
static uint8_t spi_sram_xfer_byte(uint8_t out)
{
    uint8_t in = 0;
    for (int b = 7; b >= 0; --b)
    {
        // Set MOSI according to bit
        if (out & (1U << b))
            gpio_set(SPI_SRAM_PORT, SPI_SRAM_MOSI_PIN);
        else
            gpio_clear(SPI_SRAM_PORT, SPI_SRAM_MOSI_PIN);

        spi_sram_delay();
        // Clock high
        gpio_set(SPI_SRAM_PORT, SPI_SRAM_SCK_PIN);
        spi_sram_delay();
        // Sample MISO
        in <<= 1;
        in |= (uint8_t)(gpio_read(SPI_SRAM_PORT, SPI_SRAM_MISO_PIN) ? 1 : 0);
        // Clock low
        gpio_clear(SPI_SRAM_PORT, SPI_SRAM_SCK_PIN);
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