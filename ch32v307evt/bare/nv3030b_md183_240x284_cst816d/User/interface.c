/*
  interface.c - low-level NV3030B bus primitives (ch32v307evt port,
  TK018F3716 240x284 module).

  TWO transports, selectable at runtime (LCD_SelectBus):

    SOFT  - bit-banged SPI on plain GPIOs, following the vendor's TK499
            soft-SPI example
            (TK499_183_TK018F3716_softSPI_captouch/project/bsp/LCD/LCD.c)
            bit for bit:

                for (i = 0; i < 8; i++) {
                    SPI_DCLK(0);              // falling edge: panel shifts
                    SPI_SDA((byte >> (7-i)) & 1);
                    SPI_DCLK(1);              // rising edge: panel samples
                }

            Removes every SPI-peripheral variable (mode, prescaler,
            NSS/SSI, overrun, pin AF mapping) and leaves only the pin
            levels. Slowest, most forgiving link - the bring-up path.

    HW    - the SPI2 peripheral, 8-bit frames, mode 3, MSB first,
            PCLK1/prescaler (36 MHz at the 144 MHz default). This is the
            vendor STM32/ESP configuration (both use mode 3, 8-bit,
            MSB-first), and it drives the panel correctly.

  Both share the same wrapped-command framing and the same TX buffer, so
  the only difference is how the bytes leave the chip.

  WRITE-ONLY BY DESIGN: the driver never reads from the panel and the
  module's MISO is not connected on this wiring. The NV3030B
  wrapped-command protocol needs no readback (no status polling, no
  RAM read), so nothing here depends on MISO - PB14 is left
  unconfigured on purpose.

  Wrapped-command framing (vendor-verbatim): WriteComm raises CS
  (settle), lowers it, then streams the 4-byte prefix 02 00 <cmd> 00;
  the frame stays open (CS low) so parameter/pixel bytes append
  contiguously. CS rises only at CS_SET()/LCD_EndData().

  Pins: SCK = PB13, MOSI = PB15, CS = PB12.
*/

#include <stdio.h>
#include <string.h>
#include "ch32v30x.h"
#include "debug.h"
#include "interface.h"
#include "lcd.h"

/* ---- pins (the board's spare SPI2 pins) ---- */
#define LCD_SCK_PIN    GPIO_Pin_13
#define LCD_MOSI_PIN   GPIO_Pin_15

/* BSHR sets, BCR clears - single store, no read-modify-write. */
#define LCD_SCK_HI()   (GPIOB->BSHR = LCD_SCK_PIN)
#define LCD_SCK_LO()   (GPIOB->BCR  = LCD_SCK_PIN)
#define LCD_MOSI_HI()  (GPIOB->BSHR = LCD_MOSI_PIN)
#define LCD_MOSI_LO()  (GPIOB->BCR  = LCD_MOSI_PIN)

/* Half-bit delay, in loop iterations. The bus is bit-banged, so SCK is
 * set purely by how long soft_spi_delay() spins; the real rate is
 * measured at init (see soft_spi_calibrate) and printed, so this is a
 * starting point, not a claim.
 *
 * Soft SPI is deliberately slow: the point of this path is to be the
 * most forgiving link, not the fastest. Raise LCD_SOFT_SPI_DIV to slow
 * it further if the checkerboard shows noise; lower it to speed up.
 * Override at build time: make LCD_SOFT_SPI_DIV=8 */
#ifndef LCD_SOFT_SPI_DIV
#define LCD_SOFT_SPI_DIV 4
#endif

/* Hardware SPI2 prescaler from PCLK1. Override with make LCD_SPI_PSC=4
 * (18 MHz) if the checkerboard shows noise at 36 MHz. */
#ifndef LCD_SPI_PSC
#define LCD_SPI_PSC 2
#endif

/* SPI_BaudRatePrescaler_<LCD_SPI_PSC> (two-level token paste). */
#define PSC_CAT_(a, b) a##b
#define PSC_CAT(a, b)  PSC_CAT_(a, b)

/* Bytes of pixel data accumulated before one burst. */
#define SPI_TX_BUF_SIZE 512U

static uint8_t  s_tx_buf[SPI_TX_BUF_SIZE];
static uint16_t s_tx_len;
static uint8_t  s_bus_hw;        /* 0 = soft SPI, 1 = hardware SPI2 */
static uint8_t  s_soft_ready;
static uint8_t  s_hw_ready;
static uint32_t s_soft_khz;      /* measured soft SCK, kHz */
static uint32_t s_pclk1_hz;

/* ---------------- soft (bit-banged) SPI ------------------------------ */

/* One half SCLK period. */
static void soft_spi_delay(void)
{
    volatile uint32_t n = (uint32_t)LCD_SOFT_SPI_DIV;

    while (n-- != 0U)
    {
        ;
    }
}

/* Shift one byte out, MSB first, mode 3 (vendor-verbatim sequence):
 * SCK falls, data is set, SCK rises (the panel samples here). SCK is
 * left high, which is the mode-3 idle level, so the next byte's first
 * falling edge is a real edge. */
static void soft_spi_byte(uint8_t b)
{
    uint8_t i;

    for (i = 0; i < 8U; i++)
    {
        LCD_SCK_LO();                       /* falling edge: panel shifts */
        if ((b & 0x80U) != 0U) { LCD_MOSI_HI(); } else { LCD_MOSI_LO(); }
        soft_spi_delay();
        LCD_SCK_HI();                       /* rising edge: panel samples */
        soft_spi_delay();
        b = (uint8_t)(b << 1);
    }
}

/* Measure the real SCK the bit-bang loop produces, so the console and
 * the info page report a number that was actually observed rather than
 * a guess about loop cycles.
 *
 * A whole byte is timed, not just soft_spi_delay(): the bit period is
 * the delay plus the GPIO stores and loop overhead, so timing only the
 * delay loop reports a rate far above what the bus actually runs.
 *
 * The measurement uses the SysTick millisecond counter over many bytes
 * (the RISC-V `cycle` CSR is not dependable on this core - it reads as
 * if it never advanced). SCK toggling with CS high is harmless, so this
 * runs before any frame is opened. */
static void soft_spi_calibrate(void)
{
    uint32_t t0, t1, ms;
    uint32_t n = 20000UL;
    uint32_t i;

    t0 = Get_SysTick_MS();
    for (i = 0; i < n; i++)
    {
        soft_spi_byte (0xAAU);
    }
    t1 = Get_SysTick_MS();

    ms = t1 - t0;
    if (ms == 0U)
    {
        ms = 1U;
    }

    /* bytes/s = n / (ms/1000); SCK = bytes/s * 8. */
    s_soft_khz = (uint32_t)((uint64_t)n * 8ULL * 1000ULL /
                            ((uint64_t)ms * 1000ULL));
}

/* ---------------- hardware SPI2 -------------------------------------- */

/* Push the buffered bytes through SPI2 (blocking). TXE polling feeds the
 * transmitter; BSY-clear marks the shift register empty, then the RX
 * side is drained (we never read, but the overrun flag must not latch). */
static void hw_send_stream(void)
{
    uint16_t i;

    for (i = 0; i < s_tx_len; i++)
    {
        while (SPI_I2S_GetFlagStatus (SPI2, SPI_I2S_FLAG_TXE) == RESET);
        SPI_I2S_SendData (SPI2, s_tx_buf[i]);
    }
    s_tx_len = 0U;

    while (SPI_I2S_GetFlagStatus (SPI2, SPI_I2S_FLAG_BSY) != RESET);
    while (SPI_I2S_GetFlagStatus (SPI2, SPI_I2S_FLAG_RXNE) != RESET)
    {
        (void)SPI_I2S_ReceiveData (SPI2);
    }
}

/* ---------------- shared dispatch ------------------------------------ */

/* Push all buffered bytes out (blocking), on the selected bus. */
static void spi_send_stream(void)
{
    if (s_bus_hw != 0U)
    {
        hw_send_stream();
    }
    else
    {
        uint16_t i;

        for (i = 0; i < s_tx_len; i++)
        {
            soft_spi_byte (s_tx_buf[i]);
        }
        s_tx_len = 0U;
    }
}

/* Push all buffered bytes out (blocking). */
void SPI_HW_Flush(void)
{
    if (s_tx_len != 0U)
    {
        spi_send_stream();
    }
}

void CS_SET(void)
{
    LCD_CS_SET;
}
void CS_CLR(void)
{
    LCD_CS_CLR;
}

/* ---------------- bus selection / init ------------------------------- */

/* Select the transport. Switching to soft releases SPI2 (so the
 * peripheral stops driving SCK/MOSI) and hands the pins back to GPIO;
 * switching to hardware re-enables whichever peripheral owns them.
 * Call LCD_UseHwBus() afterwards to (re)configure the pins. */
void LCD_SelectBus(uint8_t hw)
{
    SPI_HW_Flush();                 /* never switch mid-frame */

    s_bus_hw = (hw != 0U) ? 1U : 0U;

    if ((s_bus_hw == 0U) && (s_hw_ready != 0U))
    {
        SPI_Cmd (SPI2, DISABLE);    /* release SCK/MOSI to GPIO control */
    }
    else if ((s_bus_hw != 0U) && (s_hw_ready != 0U))
    {
        SPI_Cmd (SPI2, ENABLE);
    }
}

uint8_t LCD_GetBus(void)
{
    return s_bus_hw;
}

/* Configure the soft-SPI pins. ALWAYS reconfigures them: the pins are
 * shared with the SPI2 alternate function, so after a hardware pass
 * they come back as AF_PP and must be taken back as plain GPIOs. The
 * (slow) rate calibration only runs once. */
static void soft_spi_pins_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd (RCC_APB2Periph_GPIOB, ENABLE);

    /* SCK = PB13, MOSI = PB15 as plain push-pull outputs.
     * MISO = PB14 is deliberately NOT configured: the module's MISO is
     * not connected on this wiring and the driver is write-only by
     * design, so it must not depend on that line. */
    GPIO_InitStructure.GPIO_Pin   = LCD_SCK_PIN | LCD_MOSI_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init (GPIOB, &GPIO_InitStructure);

    /* Mode-3 idle: SCK high, MOSI low. */
    LCD_SCK_HI();
    LCD_MOSI_LO();

    if (s_soft_ready == 0U)
    {
        soft_spi_calibrate();
        s_soft_ready = 1U;

        printf ("[SPI] soft SPI (bit-banged) SCK=PB13 MOSI=PB15 CS=PB12\r\n");
        printf ("[SPI] div=%u  measured SCK=%lu kHz\r\n",
                (unsigned)LCD_SOFT_SPI_DIV, s_soft_khz);
    }
}

/* Configure + init SPI2. ALWAYS reconfigures the pins (they are shared
 * with the soft-SPI GPIO use), but only prints the banner once. */
static void hw_spi_pins_init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure = {0};
    SPI_InitTypeDef   SPI_InitStructure  = {0};
    RCC_ClocksTypeDef clocks;

    RCC_GetClocksFreq (&clocks);
    s_pclk1_hz = clocks.PCLK1_Frequency;

    RCC_APB2PeriphClockCmd (RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd (RCC_APB1Periph_SPI2, ENABLE);

    /* SCK = PB13, MOSI = PB15 (alternate push-pull).
     * MISO = PB14 is deliberately NOT configured: the module's MISO is
     * not connected on this wiring and the driver is write-only by
     * design, so it must not depend on that line. */
    GPIO_InitStructure.GPIO_Pin   = LCD_SCK_PIN | LCD_MOSI_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init (GPIOB, &GPIO_InitStructure);

    SPI_InitStructure.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode              = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize          = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL              = SPI_CPOL_High;     /* mode 3 */
    SPI_InitStructure.SPI_CPHA              = SPI_CPHA_2Edge;
    SPI_InitStructure.SPI_NSS               = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = PSC_CAT (SPI_BaudRatePrescaler_, LCD_SPI_PSC);
    SPI_InitStructure.SPI_FirstBit          = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial     = 7;
    SPI_Init (SPI2, &SPI_InitStructure);
    SPI_Cmd (SPI2, ENABLE);

    if (s_hw_ready == 0U)
    {
        s_hw_ready = 1U;

        printf ("[SPI] hardware SPI2 SCK=PB13 MOSI=PB15 CS=PB12\r\n");
        printf ("[SPI] target=%lu kHz  real=%lu kHz  PCLK1=%lu MHz\r\n",
                (unsigned long)(s_pclk1_hz / (unsigned long)LCD_SPI_PSC / 1000UL),
                (unsigned long)(s_pclk1_hz / (unsigned long)LCD_SPI_PSC / 1000UL),
                (unsigned long)(s_pclk1_hz / 1000000UL));
    }
}

/* Public init: configure the pins for the currently selected bus
 * (idempotent per bus). */
void LCD_UseHwBus(void)
{
    if (s_bus_hw != 0U)
    {
        hw_spi_pins_init();
    }
    else
    {
        soft_spi_pins_init();
    }
}

/* Dump the bus state (bring-up diagnostic). */
void LCD_BusDump(void)
{
    if (s_bus_hw != 0U)
    {
        printf ("[SPI] HW SPI2: CTLR1=0x%04X STATR=0x%04X real=%lu kHz (PCLK1 %lu MHz / %u)\r\n",
                (unsigned)SPI2->CTLR1, (unsigned)SPI2->STATR,
                LCD_HwSpiKHz(),
                (unsigned long)(s_pclk1_hz / 1000000UL), (unsigned)LCD_SPI_PSC);
    }
    else
    {
        printf ("[SPI] soft SPI: SCK=PB13 MOSI=PB15 CS=PB12  div=%u  SCK=%lu kHz\r\n",
                (unsigned)LCD_SOFT_SPI_DIV, LCD_HwSpiKHz());
    }
}

/* Active SCK in kHz (for the info page). */
unsigned long LCD_HwSpiKHz(void)
{
    if (s_bus_hw != 0U)
    {
        return (unsigned long)(s_pclk1_hz / (unsigned long)LCD_SPI_PSC / 1000UL);
    }
    return (unsigned long)s_soft_khz;
}

/* ---------------- byte-level transfers ------------------------------- */

/* NV3030B wrapped command: CS high (settle), CS low, then the 4-byte
 * prefix 02 00 <cmd> 00. The frame stays open for the data bytes that
 * follow (vendor-verbatim framing). */
void WriteComm(uint16_t data)
{
    SPI_HW_Flush();
    CS_SET();
    Delay_Us (5);                  /* CS high settle */
    CS_CLR();

    s_tx_buf[0] = 0x02U;
    s_tx_buf[1] = 0x00U;
    s_tx_buf[2] = (uint8_t)data;
    s_tx_buf[3] = 0x00U;
    s_tx_len    = 4U;

    spi_send_stream();
}

/* Write one data byte into the open frame. */
void WriteData(uint16_t data)
{
    s_tx_buf[s_tx_len++] = (uint8_t)data;
    if (s_tx_len >= SPI_TX_BUF_SIZE)
    {
        SPI_HW_Flush();
    }
}

/* Stream one RGB565 pixel color as two bytes into the open frame. */
void SendData(uint32_t color)
{
    s_tx_buf[s_tx_len++] = (uint8_t)(color >> 8);
    s_tx_buf[s_tx_len++] = (uint8_t)color;
    if (s_tx_len >= (SPI_TX_BUF_SIZE - 2U))
    {
        SPI_HW_Flush();
    }
}

/* Fast raw 8-bit data byte: streams into the open frame. */
void LCD_WriteDataFast(uint8_t data)
{
    s_tx_buf[s_tx_len++] = data;
    if (s_tx_len >= SPI_TX_BUF_SIZE)
    {
        SPI_HW_Flush();
    }
}

/* Solid-color bulk burst into the open frame, in buffered chunks. */
void LCD_FillBulk(uint32_t color, uint32_t pixels)
{
    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)color;

    while (pixels-- != 0U)
    {
        s_tx_buf[s_tx_len++] = hi;
        s_tx_buf[s_tx_len++] = lo;
        if (s_tx_len >= SPI_TX_BUF_SIZE)
        {
            SPI_HW_Flush();
        }
    }
    SPI_HW_Flush();
}

/* Begin/end a raster burst. The caller issues WriteComm(0x2C) first;
 * BeginData just makes sure CS is low and the burst stays in one frame;
 * EndData closes it. */
void LCD_BeginData(void)
{
    SPI_HW_Flush();
    CS_CLR();
}
void LCD_EndData(void)
{
    SPI_HW_Flush();
    CS_SET();
}
