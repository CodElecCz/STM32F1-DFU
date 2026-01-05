#include "spi_sram.h"
#include "gpio.h"
#include "board.h"
#include <stddef.h>
#include <stdint.h>

// Small delay for bit-bang timing. Tunable as needed for target speed.
static void spi_sram_delay(void)
{
    /* Shorter, deterministic delay: enough cycles to meet timing on typical
     * STM32F1 at 72MHz for a moderate SPI bit rate. Adjust if needed.
     */
    for (volatile int i = 0; i < 20; ++i) __asm__("nop");
}

// Default weak CS hooks - these will be used if board code doesn't provide
// its own implementation. They operate the CS pin defined above.
void spi_sram_cs_assert(void) { gpio_clear(SPI_SRAM_PORT, SPI_SRAM_CS_PIN); spi_sram_delay(); }
void spi_sram_cs_deassert(void) { gpio_set(SPI_SRAM_PORT, SPI_SRAM_CS_PIN); spi_sram_delay(); }

// Ensure GPIO pins are configured. Weak so a board can override to use RAM
// initialisation or alternate configurations.
void spi_sram_hw_init(void) __attribute__((weak));
void spi_sram_hw_init(void)
{
    // Enable GPIO clock for the SRAM port
    rcc_gpio_enable(SPI_SRAM_PORT);

    // Configure SCK, MOSI and CS as outputs; MISO as input with pull-up.
    gpio_set_output(SPI_SRAM_PORT, SPI_SRAM_SCK_PIN);
    gpio_set_output(SPI_SRAM_PORT, SPI_SRAM_MOSI_PIN);
    // Ensure pull-up is enabled on MISO input so the line is stable when idle.
    gpio_set(SPI_SRAM_PORT, SPI_SRAM_MISO_PIN); /* set ODR bit to enable pull-up */
    gpio_set_input_pp(SPI_SRAM_PORT, SPI_SRAM_MISO_PIN);
    gpio_set_output(SPI_SRAM_PORT, SPI_SRAM_CS_PIN);
    // Idle lines: SCK low, CS high
    gpio_clear(SPI_SRAM_PORT, SPI_SRAM_SCK_PIN);
    gpio_set(SPI_SRAM_PORT, SPI_SRAM_CS_PIN);

    spi_sram_delay();
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

// Write implementation for SPI SRAM (23LCV): sequential WRITE (0x02) command
int spi_sram_write(uint32_t addr, const void *buf, uint32_t len)
{
    if (!buf || !len)
        return -1;

    const uint8_t CMD_WRITE = 0x02;
    const uint8_t *src = (const uint8_t*)buf;

    spi_sram_cs_assert();

    // Send WRITE opcode and 24-bit address (MSB first)
    (void)spi_sram_xfer_byte(CMD_WRITE);
    (void)spi_sram_xfer_byte((uint8_t)((addr >> 16) & 0xFF));
    (void)spi_sram_xfer_byte((uint8_t)((addr >> 8) & 0xFF));
    (void)spi_sram_xfer_byte((uint8_t)((addr >> 0) & 0xFF));

    // Write data bytes
    for (uint32_t i = 0; i < len; ++i)
    {
        (void)spi_sram_xfer_byte(src[i]);
    }

    spi_sram_cs_deassert();

    return 0;
}

// Optional small self-test routine to assist debugging. Writes a pattern and reads
// it back; returns 0 on success, non-zero on failure.
int spi_sram_self_test(void)
{
    uint8_t test_write[16];
    uint8_t test_read[16];
    for (int i = 0; i < 16; ++i) test_write[i] = (uint8_t)(0xA5 ^ i);

    if (spi_sram_write(0x0000, test_write, sizeof(test_write)) != 0)
        return -1;

    if (spi_sram_read(0x0000, test_read, sizeof(test_read)) != 0)
        return -2;

    for (int i = 0; i < 16; ++i)
        if (test_read[i] != test_write[i])
            return 3 + i;

    return 0;
}
