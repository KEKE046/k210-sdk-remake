/* Copyright 2018 Canaan Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include "aes.h"
#include "fpioa.h"
#include "lcd.h"
#include "sysctl.h"
#include "nt35310.h"
#include "board_config.h"
#include "image2.h"
#include "unistd.h"
#include "gpiohs.h"

static void io_set_power(void)
{
    sysctl_set_power_mode(SYSCTL_POWER_BANK6, SYSCTL_POWER_V18);
    sysctl_set_power_mode(SYSCTL_POWER_BANK7, SYSCTL_POWER_V18);
}

static void io_init(void)
{
    fpioa_set_function(LCD_DC_PIN, FUNC_GPIOHS0 + LCD_DC_IO);
    fpioa_set_function(LCD_CS_PIN, FUNC_SPI0_SS3);
    fpioa_set_function(LCD_RW_PIN, FUNC_SPI0_SCLK);
    fpioa_set_function(LCD_RST_PIN, FUNC_GPIOHS0 + LCD_RST_IO);

    sysctl_set_spi0_dvp_data(1);

    // LCD Backlight
    fpioa_set_function(LCD_BLIGHT_PIN, FUNC_GPIOHS0 + LCD_BLIGHT_IO);
    gpiohs_set_drive_mode(LCD_BLIGHT_IO, GPIO_DM_OUTPUT);
    gpiohs_set_pin(LCD_BLIGHT_IO, GPIO_PV_LOW);
}
uint32_t enc_image[38400] __attribute__((aligned(64)));

int main(void)
{
    printf("lcd test\n");
    io_init();
    io_set_power();
    lcd_init();
    lcd_set_direction(DIR_YX_RLDU);
    uint8_t input_key[32];
    uint8_t gcm_tag[4];
    for(size_t i = 0; i < 32; i++) {
        input_key[i] = i;
    }
    for(size_t i = 0; ; i++) {
        input_key[0] = i;
        lcd_draw_picture(0, 0, 320, 240, rgb_image);
        usleep(1000000);
        aes_ecb256_hard_encrypt(input_key, rgb_image, sizeof(rgb_image), enc_image);
        lcd_draw_picture(0, 0, 320, 240, enc_image);
        usleep(1000000);
    }
}
