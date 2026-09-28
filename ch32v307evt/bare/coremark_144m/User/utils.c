#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "custom_def.h"
#include "utils.h"
#include "debug.h"

uint32_t HAL_GetTick(void) { return Get_SysTick_MS(); }

void HAL_Delay(uint32_t t) {
  Delay_MS(t);
}
