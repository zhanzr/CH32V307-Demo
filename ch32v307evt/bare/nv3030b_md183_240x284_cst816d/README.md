# nv3030b_md183_240x284_cst816d (ch32v307evt)

**TK018F3716 touch LCD module** (NV3030B 1.83" 240x284 panel + CST816D
capacitive touch) on the CH32V307 EVT board, ported from the tc212-kit
project of the same name. Same demo loop: big-font banner, checkerboard
stress pattern, vendor TEST_STAND screens with timed solid fills, info
pages (normal + inverted), HSV gradient sweep, LED test - live FPS counter
throughout, all phases printed on the USART1 console (115200).

## Wiring (the board's spare SPI2 / I2C2, see the board README)

| Signal | Pin  | Soft path                        | Hardware path           |
|--------|------|----------------------------------|-------------------------|
| SCK    | PB13 | bit-banged GPIO, mode 3, ~1.8 MHz| SPI2_SCK, 36 MHz        |
| MOSI   | PB15 | bit-banged GPIO                  | SPI2_MOSI               |
| CS     | PB12 | GPIO software CS                 | GPIO software CS        |
| MISO   | PB14 | **NOT CONNECTED** - write-only driver, by design            |
| SCL    | PB10 | bit-banged I2C, push-pull        | I2C2_SCL (AF_OD)        |
| SDA    | PB11 | bit-banged I2C, open-drain       | I2C2_SDA (AF_OD)        |

The module has no DC/reset/backlight pin in use (wrapped-command SPI
protocol); MISO is not wired and the driver never reads from the panel (the
NV3030B wrapped-command protocol needs no readback), so nothing depends on
that line.

## Two transports, alternating

Both the LCD and the touch bus have a **soft** (bit-banged) and a
**hardware** implementation, selected at runtime, and the demo alternates
between them:

- **Soft SPI** follows the vendor's TK499 example
  (`TK499_183_TK018F3716_softSPI_captouch/project/bsp/LCD/LCD.c`) bit for
  bit - SCK falls, data is set, SCK rises. SCK is ~1.8 MHz
  (`LCD_SOFT_SPI_DIV=4`; raise it to slow down, lower it to speed up).
- **Hardware SPI2**: 8-bit frames, mode 3, MSB first, 36 MHz
  (`LCD_SPI_PSC=2`; the vendor STM32/ESP configuration).
- **Soft I2C** for touch: SDA open-drain, SCL push-pull.
- **Hardware I2C2** for touch: 7-bit master mode; I2C2's fixed pins are
  exactly PB10/PB11, so the same wiring serves both.

Each transport gets a full pass: switch bus -> bring the panel up from
scratch (`LCD_Init`) -> banner -> checkerboard stress -> TEST_STAND ->
info pages -> gradient -> LED test. The panel is re-initialised every pass,
so neither transport inherits the other's setup, and the pins are
re-handed-off between the GPIO and alternate-function uses on each switch.

## Result (measured, 2026-09-29)

```
==== ch32v307evt (CH32V307) nv3030b_md183_240x284_cst816d @ 144 MHz ====
Alternating SOFT (bit-banged) and HARDWARE (SPI2/I2C2) passes.

[0ms] ======== soft SPI pass ========
[SPI] soft SPI (bit-banged) SCK=PB13 MOSI=PB15 CS=PB12
[SPI] div=4  measured SCK=1777 kHz
[TOUCH] CST816D soft I2C target=400 kHz
[TOUCH] bus scan: 0x15  (1 device)
[TOUCH] self-test: ACK (chip present)
[LCD] solid fills (ms): RED=619 GREEN=619 BLUE=617 WHITE=609 BLACK=615

[38656ms] ======== HW SPI2 pass ========
[SPI] hardware SPI2 SCK=PB13 MOSI=PB15 CS=PB12
[SPI] target=36000 kHz  real=36000 kHz  PCLK1=72 MHz
[TOUCH] CST816D hardware I2C2 SCL=PB10 SDA=PB11 target=400 kHz
[TOUCH] bus scan (HW): 0x15  (1 device)
[TOUCH] self-test: ACK (chip present)
[LCD] solid fills (ms): RED=39 GREEN=39 BLUE=39 WHITE=39 BLACK=39
```

**Both transports drive the panel correctly**, and the demo alternates
between them indefinitely. Hardware SPI2 is ~16x faster (39 ms vs 619 ms
per full-screen fill). Touch ACKs at 0x15 and reports coordinates on both
the soft and the hardware I2C2 bus.

## Demo loop

1. Per transport: bus switch, panel bring-up (draw/re-init cycles tile a
   64x64 asset over the screen - the artwork appearing IS the "panel is
   up" signal), then the smiley banner naming the transport.
2. Then the pattern set: checkerboard stress pattern -> TEST_STAND vendor
   screens (frame, gray ramp, band, 5 timed solid fills) -> info page
   (normal + inverted) -> HSV gradient sweep -> LED test (PA0). Live FPS
   counter in the bottom band; touch down/release printed on the console
   throughout.
3. Then the next transport, from scratch, forever.

## Build / Flash

```
make          # build build/nv3030b_md183_240x284_cst816d.elf
make hex      # build build/nv3030b_md183_240x284_cst816d.hex
make flash    # program via WCH-Link + OpenOCD
make size
make clean
```
