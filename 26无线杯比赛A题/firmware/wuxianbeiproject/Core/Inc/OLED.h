#ifndef OLED_HAL_H
#define OLED_HAL_H

#include "main.h"

HAL_StatusTypeDef OLED_Init(void);
HAL_StatusTypeDef OLED_Clear(void);
/* Line: 1..4, Column: 1..16; English letters, digits and punctuation. */
HAL_StatusTypeDef OLED_ShowString(uint8_t Line, uint8_t Column, const char *String);

#endif
