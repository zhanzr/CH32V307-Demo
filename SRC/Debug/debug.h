/********************************** (C) COPYRIGHT  *******************************
* File Name          : debug.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2021/06/06
* Description        : This file contains all the functions prototypes for UART
*                      Printf , Delay functions.
*********************************************************************************
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for 
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/
#ifndef __DEBUG_H
#define __DEBUG_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "stdio.h"
#include "ch32v30x.h"

/* UART Printf Definition */
#define DEBUG_UART1    1
#define DEBUG_UART2    2
#define DEBUG_UART3    3

/* DEBUG UATR Definition */
#ifndef DEBUG
#define DEBUG   DEBUG_UART1
#endif

/* SDI Printf Definition */
#define SDI_PR_CLOSE   0
#define SDI_PR_OPEN    1

#ifndef SDI_PRINT
#define SDI_PRINT   SDI_PR_CLOSE
#endif

/**
 * @brief  Initializes the SysTick peripheral to generate a 1ms tick interrupt.
 * @note   SystemCoreClock must be updated before calling this function.
 */
void SysTick_Init(void);

/**
 * @brief  Gets the current millisecond counter value.
 * @return 32-bit unsigned integer representing milliseconds elapsed.
 */
uint32_t Get_SysTick_MS(void);

/**
 * @brief  Blocking delay in milliseconds using the SysTick tick counter.
 * @param  ms Number of milliseconds to delay.
 */
void Delay_MS(uint32_t ms);

/**
 * @brief  Resets the millisecond counter back to zero.
 */
void Clear_SysTick_MS(void);

void USART_Printf_Init(uint32_t baudrate);
void SDI_Printf_Enable(void);

#ifdef __cplusplus
}
#endif

#endif 



