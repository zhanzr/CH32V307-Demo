/*
  interface.h - low-level NV3030B bus primitives (ch32v307evt port,
  TK018F3716 240x284 module).

  The NV3030B wrapped-command protocol: every command is written as
  CS high (settle), CS low, then the 4-byte prefix 02 00 <cmd> 00;
  parameter and pixel bytes then stream into the same CS frame. The
  module's DC pin is not part of this protocol (driven low by the
  vendor / left floating) and there is no reset pin - power-cycling
  the module is the only recovery from a latched state.

  Transport: TWO selectable paths (LCD_SelectBus), both 8-bit frames,
  SPI mode 3, MSB first:

    SOFT - bit-banged SPI on plain GPIOs, ~1.8 MHz (LCD_SOFT_SPI_DIV in
           interface.c). Follows the vendor's TK499 soft-SPI example bit
           for bit; removes every SPI-peripheral variable and leaves only
           the pin levels. The bring-up path.
    HW   - the SPI2 peripheral, PCLK1/prescaler (36 MHz at the 144 MHz
           default, LCD_SPI_PSC). The vendor STM32/ESP configuration.

    SCK  = PB13
    MOSI = PB15
    MISO = PB14 - NOT CONNECTED (write-only driver, by design)
    CS   = PB12 (GPIO output)
*/

#ifndef __INTERFACE_H
#define __INTERFACE_H

#include <stdint.h>

void CS_SET(void);
void CS_CLR(void);
void WriteComm(uint16_t data);
void WriteData(uint16_t data);
void SendData(uint32_t color);
void LCD_WriteDataFast(uint8_t data);   /* raw byte, caller manages framing */
void LCD_BeginData(void);                /* CS low, ready for raster bytes */
void LCD_EndData(void);                  /* CS high, closes the frame */
void LCD_FillBulk(uint32_t color, uint32_t pixels); /* solid burst, open frame */

unsigned long LCD_HwSpiKHz(void); /* active SCK in kHz (info page)          */
void    SPI_HW_Flush(void);     /* drain the TX buffer (blocking)         */
void    LCD_UseHwBus(void);     /* pin init for the selected bus (idempotent) */
void    LCD_BusDump(void);      /* print bus state                        */

/* ---- transport selection ---- */
#define LCD_BUS_SOFT  0u        /* bit-banged SPI on GPIOs                */
#define LCD_BUS_HW    1u        /* SPI2 peripheral                        */
void    LCD_SelectBus(uint8_t hw);  /* 0 = soft, 1 = hardware             */
uint8_t LCD_GetBus(void);           /* current selection                  */

#endif /* __INTERFACE_H */
