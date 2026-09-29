/*
  touch.c - CST816D capacitive touch for the ch32v307evt TK018F3716
  module port.

  TWO transports, selectable at runtime (Touch_SelectBus):

    SOFT - bit-banged I2C. SDA is open-drain (release = input with
           pull-up, the module's pull-up drives the line), SCL is
           push-pull (master-only clock). Matches the vendor STM32
           example (hard_spi_captouch/bsp/touch_CTP), which also
           bit-bangs the bus. No peripheral to misconfigure.

    HW   - the I2C2 peripheral in 7-bit master mode. The pins are the
           same (I2C2's fixed mapping is exactly PB10/PB11), configured
           AF_OD. Reads use the classic register-pointer sequence:
           START + addr(W) + reg 0x00 + STOP, then
           START + addr(R) + read N bytes + NACK/STOP.

  Both read the same touch data block from register 0x00; byte 3
  (register 0x03) = 0x80 marks an active touch. 7-bit slave address
  0x15, so the wire byte is 0x2A for write / 0x2B for read.

  Wiring (the spare buses picked for this board): SCL = PB10,
  SDA = PB11 (the I2C2 pins).
*/
#include <stdio.h>
#include "touch.h"
#include "debug.h"
#include "ch32v30x.h"

/* Touch bus GPIOs: SCL = PB10, SDA = PB11.
 * Overridable at build time to test wiring orientations:
 *   make LCD_SOFT_SPI_DIV=... EXTRA_DEFS="-DT_SCL_PIN=GPIO_Pin_11 -DT_SDA_PIN=GPIO_Pin_10" (swapped)
 */
#ifndef T_SCL_PIN
#define T_SCL_PIN       GPIO_Pin_10
#endif
#ifndef T_SDA_PIN
#define T_SDA_PIN       GPIO_Pin_11
#endif
#define T_PORT          GPIOB
#define T_RCC           RCC_APB2Periph_GPIOB

/* 7-bit slave address 0x15 -> wire byte 0x2A (write) / 0x2B (read). */
#define CTP_ADDR_W      0x2AU
#define CTP_ADDR_R      0x2BU

/* Stringify the compiled-in pin config, so the banner reports what is
 * ACTUALLY built rather than a hardcoded string. */
#define T_STR(x)        #x
#define T_XSTR(x)       T_STR(x)

#define T_SCL_HI()      GPIO_WriteBit (T_PORT, T_SCL_PIN, Bit_SET)
#define T_SCL_LO()      GPIO_WriteBit (T_PORT, T_SCL_PIN, Bit_RESET)
#define T_SDA_HI()      GPIO_WriteBit (T_PORT, T_SDA_PIN, Bit_SET)
#define T_SDA_LO()      GPIO_WriteBit (T_PORT, T_SDA_PIN, Bit_RESET)
#define T_SDA_VAL()     GPIO_ReadInputDataBit (T_PORT, T_SDA_PIN)

/* SCL frequency target.
 *
 * The bus is bit-banged, so the SCL period is set by t_delay(). Here the
 * shared Delay_Us() busy loop is used: one call per half bit gives a
 * real SCL of a few hundred kHz (the loop overhead at 144 MHz lands
 * close to the Fast-mode 1.25 us half bit). CST816D accepts 100-400
 * kHz, so no calibration loop is needed. */
#ifndef T_I2C_HZ
#define T_I2C_HZ        400000UL    /* CST816D Fast-mode maximum */
#endif

/* Wait one half SCL period. */
static void t_delay(void)
{
    Delay_Us (1);
}

/* ---- transport selection ---- */
static uint8_t s_bus_hw;         /* 0 = soft I2C, 1 = hardware I2C2 */
static uint8_t s_hw_ready;

/* Hardware-I2C helpers, used by the bring-up probes further down before
 * their definitions. */
static uint8_t i2c_wait_event(uint32_t event);
static uint8_t i2c_hw_probe(void);

/* Configuration figure: the rate that was requested. The bit-banged SCL
 * rate is quantised by the busy-loop granularity, so deriving it back
 * from timing would misreport; the hardware rate is whatever I2C2 was
 * programmed with. Either way this is the request. */
uint32_t Touch_GetHz(void)
{
    return T_I2C_HZ;
}

static void t_sda_out_od(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    GPIO_InitStructure.GPIO_Pin  = T_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init (T_PORT, &GPIO_InitStructure);
}

static void t_sda_release(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    GPIO_InitStructure.GPIO_Pin  = T_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init (T_PORT, &GPIO_InitStructure);
}

static void t_scl_out_pp(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    GPIO_InitStructure.GPIO_Pin   = T_SCL_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init (T_PORT, &GPIO_InitStructure);
}

static void soft_start(void)
{
    T_SDA_HI();
    T_SCL_HI();
    t_delay();
    T_SDA_LO();     /* START: SDA falls while SCL is high */
    t_delay();
    T_SCL_LO();
    t_delay();
}

static void soft_stop(void)
{
    T_SDA_LO();
    t_delay();
    T_SCL_HI();
    t_delay();
    T_SDA_HI();     /* STOP: SDA rises while SCL is high */
    t_delay();
}

/* Write one byte MSB first. Returns 1 if the slave ACKed. */
static uint8_t soft_write_byte(uint8_t dat)
{
    uint8_t ack;
    uint8_t i;

    for (i = 0; i < 8u; i++)
    {
        if ((dat & 0x80u) != 0u) { T_SDA_HI(); } else { T_SDA_LO(); }
        t_delay();
        T_SCL_HI();
        t_delay();
        T_SCL_LO();
        t_delay();
        dat = (uint8_t)(dat << 1);
    }

    /* 9th clock: release SDA, sample the slave ACK */
    t_sda_release();
    T_SCL_HI();
    t_delay();
    ack = (T_SDA_VAL() == 0u) ? 1u : 0u;
    T_SCL_LO();
    t_delay();
    t_sda_out_od();
    return ack;
}

/* Read one byte MSB first; ACK it unless ack == 0 (last byte).
 *
 * IMPORTANT: the ACK bit is driven with SDA configured as an OUTPUT.
 * t_sda_release() puts the pin into input mode, so writing SDA before
 * restoring the output mode does nothing (the pin has no driver) and
 * the slave never sees the ACK. It then releases SDA, the pull-up
 * wins, and every following byte reads back as 0xFF - the classic
 * "byte0 = <id>, byte1..n = FF" symptom. Drive SDA low FIRST, then
 * clock the 9th bit. */
static uint8_t soft_read_byte(uint8_t ack)
{
    uint8_t dat = 0u;
    uint8_t i;

    t_sda_release();
    for (i = 0; i < 8u; i++)
    {
        T_SCL_LO();
        t_delay();
        T_SCL_HI();
        t_delay();
        dat = (uint8_t)((dat << 1) | (T_SDA_VAL() != 0u ? 1u : 0u));
    }
    T_SCL_LO();
    t_delay();

    /* Master ACK/NACK: needs SDA driven, so restore the output mode
     * before setting the level. */
    t_sda_out_od();
    if (ack != 0u) { T_SDA_LO(); } else { T_SDA_HI(); }
    t_delay();
    T_SCL_HI();
    t_delay();
    T_SCL_LO();
    t_delay();
    return dat;
}

/* Bring-up check: probe only the address byte and report the ACK, so a
 * wiring/address problem is distinguishable from "no finger down". */
uint8_t Touch_SelfTest(void)
{
    uint8_t acked;

    if (s_bus_hw != 0u)
    {
        return i2c_hw_probe();
    }

    soft_start();
    acked = soft_write_byte(CTP_ADDR_W);
    soft_stop();

    return acked;
}

/* Probe every 7-bit address (0x01..0x7F) and list the ones that ACK.
 * Bring-up tool for a new wiring: distinguishes "nothing on the bus"
 * (wiring/power problem) from "device at another address" (list shows
 * it). */
void Touch_Scan(void)
{
    uint8_t addr;
    uint8_t found = 0u;

    if (s_bus_hw != 0u)
    {
        printf ("[TOUCH] bus scan (HW):");
        for (addr = 1u; addr < 120u; addr++)
        {
            uint8_t ack = 1u;

            I2C_GenerateSTART (I2C2, ENABLE);
            if (i2c_wait_event (I2C_EVENT_MASTER_MODE_SELECT) == 0u) { ack = 0u; }

            if (ack != 0u)
            {
                I2C_Send7bitAddress (I2C2, (uint8_t)(addr << 1),
                                     I2C_Direction_Transmitter);
                if (i2c_wait_event (I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) == 0u)
                {
                    ack = 0u;
                }
            }

            I2C_GenerateSTOP (I2C2, ENABLE);

            if (ack != 0u)
            {
                printf (" 0x%02X", addr);
                found++;
            }
        }
        printf ("  (%u device%s)\r\n", (unsigned)found, (found == 1u) ? "" : "s");
        return;
    }

    printf ("[TOUCH] bus scan:");
    for (addr = 1u; addr < 120u; addr++)
    {
        uint8_t ack;

        soft_start();
        ack    = soft_write_byte((uint8_t)(addr << 1));
        ack   |= soft_write_byte(0x00u);   /* second byte: many chips only ACK pairs */
        soft_stop();

        if (ack != 0u)
        {
            printf (" 0x%02X", addr);
            found++;
        }
    }
    printf ("  (%u device%s)\r\n", (unsigned)found, (found == 1u) ? "" : "s");
}

/* =====================================================================
   Hardware I2C2 path.
   ===================================================================== */

/* Bounded waits, so a missing device (no ACK) shows up as a failed
 * transfer instead of a hang. */
#define I2C_HW_TIMEOUT  200000UL

static uint8_t i2c_wait_flag(uint32_t flag)
{
    uint32_t guard = 0;

    while (I2C_GetFlagStatus (I2C2, flag) == RESET)
    {
        if (++guard > I2C_HW_TIMEOUT) { return 0u; }
    }
    return 1u;
}

static uint8_t i2c_wait_event(uint32_t event)
{
    uint32_t guard = 0;

    while (I2C_CheckEvent (I2C2, event) == NoREADY)
    {
        if (++guard > I2C_HW_TIMEOUT) { return 0u; }
    }
    return 1u;
}

/* Software recovery: 9 clocks with SDA released, then a STOP, so a
 * slave that is holding SDA low from a prior aborted transfer lets go. */
static void i2c_hw_bus_recover(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    uint8_t i;

    /* Take the pins as plain GPIO. */
    I2C_Cmd (I2C2, DISABLE);

    GPIO_InitStructure.GPIO_Pin   = T_SCL_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init (GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin  = T_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_Init (GPIOB, &GPIO_InitStructure);

    T_SDA_HI();
    for (i = 0; i < 9u; i++)
    {
        T_SCL_LO(); t_delay();
        T_SCL_HI(); t_delay();
    }
    /* STOP: SDA low, then SCL high, then SDA high. */
    T_SDA_LO(); t_delay();
    T_SCL_HI(); t_delay();
    T_SDA_HI(); t_delay();

    /* Back to the I2C2 alternate function. */
    GPIO_InitStructure.GPIO_Pin  = T_SCL_PIN | T_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
    GPIO_Init (GPIOB, &GPIO_InitStructure);
    I2C_Cmd (I2C2, ENABLE);
}

/* Configure I2C2 in 7-bit master mode. ALWAYS reconfigures the pins
 * (they are shared with the soft-I2C GPIO use); only prints once. */
static void i2c_hw_pins_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    I2C_InitTypeDef  I2C_InitStructure   = {0};

    RCC_APB2PeriphClockCmd (RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd (RCC_APB1Periph_I2C2, ENABLE);

    GPIO_SetBits (T_PORT, T_SCL_PIN | T_SDA_PIN);

    I2C_InitStructure.I2C_ClockSpeed          = T_I2C_HZ;
    I2C_InitStructure.I2C_Mode                = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle           = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1         = 0x00;
    I2C_InitStructure.I2C_Ack                 = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Init (I2C2, &I2C_InitStructure);
    I2C_Cmd (I2C2, ENABLE);

    /* I2C2's fixed pins: SCL = PB10, SDA = PB11, alternate open-drain. */
    GPIO_InitStructure.GPIO_Pin   = T_SCL_PIN | T_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init (T_PORT, &GPIO_InitStructure);

    if (s_hw_ready == 0U)
    {
        s_hw_ready = 1U;
        printf ("[TOUCH] CST816D hardware I2C2 SCL=PB10 SDA=PB11 target=%lu kHz\r\n",
                (unsigned long)(Touch_GetHz() / 1000UL));
    }
}

/* Probe the address byte and return 1 on ACK (with STOP). */
static uint8_t i2c_hw_probe(void)
{
    uint8_t ack = 1u;

    I2C_GenerateSTART (I2C2, ENABLE);
    if (i2c_wait_event (I2C_EVENT_MASTER_MODE_SELECT) == 0u) { ack = 0u; }

    if (ack != 0u)
    {
        I2C_Send7bitAddress (I2C2, CTP_ADDR_W, I2C_Direction_Transmitter);
        if (i2c_wait_event (I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) == 0u) { ack = 0u; }
    }

    I2C_GenerateSTOP (I2C2, ENABLE);
    return ack;
}

/* Read len bytes from register 0x00 over hardware I2C2.
 * Register-pointer phase: START + addr(W) + 0x00 + STOP; then
 * START + addr(R) + N bytes (ACK each but the last) + STOP. */
static void i2c_hw_read_block(uint8_t *buf, uint8_t len)
{
    uint8_t ok = 1u;
    uint8_t i;

    for (i = 0; i < len; i++) { buf[i] = 0u; }

    if (i2c_hw_probe() == 0u) { return; }

    /* Pointer phase. */
    I2C_GenerateSTART (I2C2, ENABLE);
    if (i2c_wait_event (I2C_EVENT_MASTER_MODE_SELECT) == 0u) { ok = 0u; }

    if (ok != 0u)
    {
        I2C_Send7bitAddress (I2C2, CTP_ADDR_W, I2C_Direction_Transmitter);
        if (i2c_wait_event (I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) == 0u) { ok = 0u; }
    }

    if (ok != 0u)
    {
        I2C_SendData (I2C2, 0x00u);
        if (i2c_wait_event (I2C_EVENT_MASTER_BYTE_TRANSMITTED) == 0u) { ok = 0u; }
    }

    I2C_GenerateSTOP (I2C2, ENABLE);

    /* Read phase. */
    if (ok != 0u)
    {
        I2C_AcknowledgeConfig (I2C2, ENABLE);

        I2C_GenerateSTART (I2C2, ENABLE);
        if (i2c_wait_event (I2C_EVENT_MASTER_MODE_SELECT) == 0u) { ok = 0u; }
    }

    if (ok != 0u)
    {
        I2C_Send7bitAddress (I2C2, CTP_ADDR_R, I2C_Direction_Receiver);
        if (i2c_wait_event (I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED) == 0u) { ok = 0u; }
    }

    for (i = 0; i < len && ok != 0u; i++)
    {
        if (i == (uint8_t)(len - 1u))
        {
            /* NACK the last byte, and STOP before reading it so the
             * master does not stretch the final byte. */
            I2C_AcknowledgeConfig (I2C2, DISABLE);
            I2C_GenerateSTOP (I2C2, ENABLE);
        }

        if (i2c_wait_flag (I2C_FLAG_RXNE) == 0u) { ok = 0u; break; }
        buf[i] = I2C_ReceiveData (I2C2);
    }

    I2C_AcknowledgeConfig (I2C2, ENABLE);

    if (ok == 0u)
    {
        for (i = 0; i < len; i++) { buf[i] = 0u; }
    }
}

/* Map the compiled-in pin constants to numbers for the banner. */
static unsigned t_pin_num(uint32_t pin)
{
    return (pin == GPIO_Pin_10) ? 10u : 11u;
}

void Touch_Init(void)
{
    RCC_APB2PeriphClockCmd (T_RCC, ENABLE);

    if (s_bus_hw != 0u)
    {
        i2c_hw_bus_recover();     /* release a stuck slave, then re-arm */
        i2c_hw_pins_init();
    }
    else
    {
        /* SCL: push-pull output; SDA: open-drain output (released). */
        t_scl_out_pp();
        t_sda_out_od();
        T_SCL_HI();
        T_SDA_HI();
        t_delay();
        soft_stop();        /* release any stuck state from a prior session */
    }

    printf ("[TOUCH] CST816D %s I2C target=%lu kHz\r\n",
            (s_bus_hw != 0u) ? "hardware" : "soft",
            (unsigned long)(Touch_GetHz() / 1000UL));
    printf ("[TOUCH] compiled pins: SDA=PB%u SCL=PB%u\r\n",
            t_pin_num (T_SDA_PIN), t_pin_num (T_SCL_PIN));
}

/* Select the transport. Switching to soft drives the pins as GPIO;
 * switching to hardware re-arms I2C2 and gives the pins back to its
 * alternate function. */
void Touch_SelectBus(uint8_t hw)
{
    s_bus_hw = (hw != 0U) ? 1U : 0U;
}

uint8_t Touch_GetBus(void)
{
    return s_bus_hw;
}

/* Read len bytes starting at register 0x00 (the CST816D touch data
 * block). On the soft bus: one write transaction sets the register
 * pointer, then a read transaction fetches the data (the CST816D
 * auto-increments). On the hardware bus i2c_hw_read_block() does the
 * equivalent sequence. On any NAK the buffer is zeroed (caller sees
 * "no touch"). */
void Touch_Read(uint8_t *buf, uint8_t len)
{
    uint8_t ok = 1u;
    uint8_t i;

    if (s_bus_hw != 0u)
    {
        i2c_hw_read_block (buf, len);
        return;
    }

    for (i = 0; i < len; i++) { buf[i] = 0u; }

    soft_start();
    if (soft_write_byte(CTP_ADDR_W) == 0u) { ok = 0u; }
    if (ok != 0u)
    {
        if (soft_write_byte(0x00u) == 0u) { ok = 0u; }
    }

    if (ok != 0u)
    {
        soft_start();
        if (soft_write_byte(CTP_ADDR_R) == 0u) { ok = 0u; }
        if (ok != 0u)
        {
            for (i = 0; i < len; i++)
            {
                buf[i] = soft_read_byte((uint8_t)(i == (len - 1u) ? 0u : 1u));
            }
        }
    }
    soft_stop();

    if (ok == 0u)
    {
        for (i = 0; i < len; i++) { buf[i] = 0u; }
    }
}
