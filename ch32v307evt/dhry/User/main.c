/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2021/06/06
 * Description        : Main program body.
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

/*
 *@Note
 GPIO routine:
 PA0 push-pull output.

*/

#include "debug.h"
#include "custom_def.h"
#include "dhry.h"
/* Global define */
extern void Proc_5 (void);

/* Global Variable */

/*********************************************************************
 * @fn      GPIO_Toggle_INIT
 *
 * @brief   Initializes GPIOA.0
 *
 * @return  none
 */
void GPIO_Toggle_INIT (void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd (RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init (GPIOA, &GPIO_InitStructure);
}

/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @return  none
 */
int main (void) {
    u8 i = 0;

    GPIO_Toggle_INIT();
    NVIC_PriorityGroupConfig (NVIC_PriorityGroup_2);
    SystemCoreClockUpdate();
    SysTick_Init();
    USART_Printf_Init (115200);

    printf ("SystemClk:%u ChipID:%08x %s\r\n", SystemCoreClock, DBGMCU_GetCHIPID(), COMPILER_NAME);
    printf ("addr: %08X %08X\n", (uint32_t)(&dhry_main), (uint32_t)(&Proc_5));

    while (1) {
        Clear_SysTick_MS();
        dhry_main (SystemCoreClock);
        printf ("clk:%u id:%08x %s\r\n", SystemCoreClock, DBGMCU_GetCHIPID(), COMPILER_NAME);
        printf ("addr: %08X %08X\n", (uint32_t)(&dhry_main), (uint32_t)(&Proc_5));

        HAL_Delay (configTICK_RATE_HZ * 20);
        GPIO_WriteBit (GPIOC, GPIO_Pin_13, (i == 0) ? (i = Bit_SET) : (i = Bit_RESET));
    }
}
