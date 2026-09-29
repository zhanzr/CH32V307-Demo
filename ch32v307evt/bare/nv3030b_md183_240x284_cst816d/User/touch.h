/*
  touch.h - CST816D capacitive touch (SCL = PB10, SDA = PB11) for the
  ch32v307evt TK018F3716 module port.

  The touch data block is read from register 0x00 (the vendor example
  reads 8 bytes from 0; registers: 0x00 Device_Mode, 0x01 GEST_ID,
  0x02 TD_STATUS, 0x03 P1_XH, 0x04 P1_XL, 0x05 P1_YH, 0x06 P1_YL).
  7-bit slave address 0x15 (the byte on the wire is 0x2A for write).

  Two selectable transports (Touch_SelectBus): bit-banged I2C, or the
  I2C2 peripheral in 7-bit master mode. I2C2's fixed pins are exactly
  PB10/PB11, so the same wiring serves both.
*/

#ifndef __TOUCH_H
#define __TOUCH_H

#include <stdint.h>

void Touch_Init(void);
void Touch_Read(uint8_t *buf, uint8_t len);

/* Bring-up check: probe the bus and return 1 if the CST816D ACKs its
 * address, 0 otherwise. Distinguishes "no finger down" (valid data,
 * buf[3] != 0x80) from "the chip never answered" (wiring/address). */
uint8_t Touch_SelfTest(void);
void    Touch_Scan(void);

/* Configuration figure: the SCL rate actually asked for. The bit-banged
 * rate is quantised by the delay's granularity, so this reports the
 * request, not a derived value. */
uint32_t Touch_GetHz(void);

/* ---- transport selection ---- */
#define TOUCH_BUS_SOFT  0u      /* bit-banged I2C                         */
#define TOUCH_BUS_HW    1u      /* I2C2 peripheral                        */
void    Touch_SelectBus(uint8_t hw);   /* 0 = soft, 1 = hardware        */
uint8_t Touch_GetBus(void);            /* current selection             */

#endif /* __TOUCH_H */
