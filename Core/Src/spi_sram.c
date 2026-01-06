#include "spi_sram.h"
#include "gpio.h"
#include "board.h"
#include <stddef.h>
#include <stdint.h>

// Small delay for bit-bang timing. Tunable as needed for target speed.
static inline void spi_sram_delay(void)
{
    /* Shorter, deterministic delay: enough cycles to meet timing on typical
     * STM32F1 at 72MHz for a moderate SPI bit rate. Adjust if needed.
     */
    for (volatile int i = 0; i < 2; ++i) __asm__("nop");
}

// Default weak CS hooks - these will be used if board code doesn't provide
// its own implementation. They operate the CS pin defined above.
void spi_sram_cs_assert(void) { gpio_clear(SPI_SRAM_PORT, SPI_SRAM_CS_PIN); spi_sram_delay(); }
void spi_sram_cs_deassert(void) { gpio_set(SPI_SRAM_PORT, SPI_SRAM_CS_PIN); spi_sram_delay(); }

// Low-level SPI2 register definitions (STM32F1 series)
#define RCC_APB1ENR    (*(volatile uint32_t*)0x4002101CU)
#define RCC_APB2ENR    (*(volatile uint32_t*)0x40021018U)

#define SPI2_BASE      0x40003800U
#define SPI2_CR1       (*(volatile uint32_t*)(SPI2_BASE + 0x00U))
#define SPI2_CR2       (*(volatile uint32_t*)(SPI2_BASE + 0x04U))
#define SPI2_SR        (*(volatile uint32_t*)(SPI2_BASE + 0x08U))
#define SPI2_DR        (*(volatile uint32_t*)(SPI2_BASE + 0x0CU))

// SPI_SR flags
#define SPI_SR_RXNE    (1U << 0)
#define SPI_SR_TXE     (1U << 1)
#define SPI_SR_BSY     (1U << 7)

// Ensure GPIO pins are configured. Weak so a board can override to use RAM
// initialisation or alternate configurations.
void spi_sram_hw_init(void) __attribute__((weak));
void spi_sram_hw_init(void)
{
    // Enable GPIO clock for the SRAM port (port is likely GPIOB for SPI2 pins)
    rcc_gpio_enable(SPI_SRAM_PORT);

    // Configure CS pin as GPIO output (keep existing behaviour)
    gpio_set_output(SPI_SRAM_PORT, SPI_SRAM_CS_PIN);
    gpio_set(SPI_SRAM_PORT, SPI_SRAM_CS_PIN); // CS idle high
    // Configure SPI2 pins (PB13=SCK, PB14=MISO, PB15=MOSI) as AF/inputs.
    // Use alternate-function push-pull for SCK and MOSI; input pull-up for MISO.
    // AF push-pull 50MHz -> mode nibble 0xB (CNF=10, MODE=11)
    gpio_set_mode(SPI_SRAM_PORT, SPI_SRAM_SCK_PIN, 0xB);
    gpio_set_mode(SPI_SRAM_PORT, SPI_SRAM_MOSI_PIN, 0xB);
    // Input pull-up/pull-down -> nibble 0x8; enable ODR pull-up bit first.
    gpio_set(SPI_SRAM_PORT, SPI_SRAM_MISO_PIN);
    gpio_set_mode(SPI_SRAM_PORT, SPI_SRAM_MISO_PIN, 0x8);
    spi_sram_delay();

    // Enable SPI2 clock on APB1 (bit 14)
    RCC_APB1ENR |= (1U << 14);
    // Enable AFIO (may be required for alternate-function pins / remap)
    RCC_APB2ENR |= (1U << 0);
    // Read back to ensure the write has taken effect on the APB bus
    (void)RCC_APB1ENR;

    // Configure SPI2: master, CPOL=0, CPHA=0, MSB-first, 8-bit, software NSS, enable
    // Set CR1 while SPI disabled. Choose baud rate PCLK/16 (BR = 0b011)    SPI2_CR1 = 0;    SPI2_CR1 |= (1U << 2); /* MSTR */    SPI2_CR1 |= (3U << 3); /* BR = 011: fPCLK/16 */    SPI2_CR1 |= (1U << 9); /* SSM = 1 */    SPI2_CR1 |= (1U << 8); /* SSI = 1 (NSS high) */    // CPOL/CPHA = 0 (mode 0), DFF=0 (8-bit)    // CR2 default is fine for simple use (NSS managed by software)    // Enable SPI peripheral    SPI2_CR1 |= (1U << 6); /* SPE */}

// Transfer a single byte using hardware SPI2 and return received bytestatic uint8_t spi_sram_xfer_byte(uint8_t out){    // Clear any stale RXNE by reading DR if set    if (SPI2_SR & SPI_SR_RXNE) {        (void)*((volatile uint16_t*)&SPI2_DR);    }    // Wait until TXE is set    while (!(SPI2_SR & SPI_SR_TXE)) { __asm__("nop"); }    // Write data (16-bit write access, lower 8-bit contains our byte)    *((volatile uint16_t*)&SPI2_DR) = (uint16_t)out;    // Wait until RXNE set (data received)    while (!(SPI2_SR & SPI_SR_RXNE)) { __asm__("nop"); }    uint8_t in = (uint8_t)(*((volatile uint16_t*)&SPI2_DR) & 0xFFU);    // Wait while SPI is busy before next transaction (optional)    while (SPI2_SR & SPI_SR_BSY) { __asm__("nop"); }    return in;}

// Transfer a single byte MSB-first and read incoming bits (mode 0: CPOL=0, CPHA=0)
static uint8_t spi_sram_gpio_xfer_byte(uint8_t out)
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
