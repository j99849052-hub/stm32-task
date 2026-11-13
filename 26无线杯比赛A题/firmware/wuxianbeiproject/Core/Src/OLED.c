#include "OLED.h"
#include "OLED_Font.h"

extern I2C_HandleTypeDef hi2c1;

#define OLED_ADDRESS (0x3CU << 1)
#define OLED_TIMEOUT_MS 100U

/* 0x00 is the OLED command control byte, not a register address. */
static HAL_StatusTypeDef OLED_WriteCommand(uint8_t *commands, uint16_t count)
{
    return HAL_I2C_Mem_Write(&hi2c1, OLED_ADDRESS, 0x00,
                             I2C_MEMADD_SIZE_8BIT, commands, count,
                             OLED_TIMEOUT_MS);
}

/* 0x40 identifies the following bytes as pixel data. */
static HAL_StatusTypeDef OLED_WriteData(uint8_t *pixels, uint16_t count)
{
    return HAL_I2C_Mem_Write(&hi2c1, OLED_ADDRESS, 0x40,
                             I2C_MEMADD_SIZE_8BIT, pixels, count,
                             OLED_TIMEOUT_MS);
}

static HAL_StatusTypeDef OLED_SetCursor(uint8_t page, uint8_t x)
{
    uint8_t commands[] = {
        (uint8_t)(0xB0U | page),
        (uint8_t)(x & 0x0FU),
        (uint8_t)(0x10U | (x >> 4))
    };
    return OLED_WriteCommand(commands, sizeof(commands));
}

HAL_StatusTypeDef OLED_Clear(void)
{
    uint8_t zeros[128] = {0};
    HAL_StatusTypeDef status;

    for (uint8_t page = 0; page < 8; page++)
    {
        status = OLED_SetCursor(page, 0);
        if (status != HAL_OK) return status;
        status = OLED_WriteData(zeros, sizeof(zeros));
        if (status != HAL_OK) return status;
    }
    return HAL_OK;
}

HAL_StatusTypeDef OLED_Init(void)
{
    /* Same 128x64 setup as the user's original driver, with page addressing. */
    uint8_t commands[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00,
        0x40, 0xA1, 0xC8, 0xDA, 0x12, 0x81, 0xCF,
        0xD9, 0xF1, 0xDB, 0x30, 0x20, 0x02,
        0xA4, 0xA6, 0x8D, 0x14
    };
    uint8_t displayOn = 0xAF;
    HAL_StatusTypeDef status;

    HAL_Delay(100);
    status = OLED_WriteCommand(commands, sizeof(commands));
    if (status != HAL_OK) return status;
    status = OLED_Clear();
    if (status != HAL_OK) return status;
    return OLED_WriteCommand(&displayOn, 1);
}

HAL_StatusTypeDef OLED_ShowString(uint8_t Line, uint8_t Column, const char *String)
{
    uint8_t count = 0;
    uint8_t pixels[128];
    HAL_StatusTypeDef status;

    if (Line < 1 || Line > 4 || Column < 1 || Column > 16 || String == 0)
        return HAL_ERROR;

    while (count < (17U - Column) && String[count] != '\0') count++;
    if (count == 0) return HAL_OK;

    for (uint8_t half = 0; half < 2; half++)
    {
        for (uint8_t i = 0; i < count; i++)
        {
            uint8_t ch = (uint8_t)String[i];
            if (ch < 32 || ch > 126) ch = '?';
            for (uint8_t x = 0; x < 8; x++)
                pixels[i * 8U + x] = OLED_F8x16[ch - 32U][half * 8U + x];
        }

        status = OLED_SetCursor((uint8_t)((Line - 1U) * 2U + half),
                                (uint8_t)((Column - 1U) * 8U));
        if (status != HAL_OK) return status;
        status = OLED_WriteData(pixels, (uint16_t)(count * 8U));
        if (status != HAL_OK) return status;
    }
    return HAL_OK;
}
