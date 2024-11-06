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
#include "my_sha256.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include "fpioa.h"
#include "lcd.h"
#include "nt35310.h"
#include "syscalls.h"
#include "sysctl.h"
#include "stdlib.h"
#include "uart.h"
#include "board_config.h"
#include "sha256.h"
#include "gpiohs.h"

uint32_t g_lcd_gram[LCD_X_MAX * LCD_Y_MAX / 2] __attribute__((aligned(128)));

static void lcd_io_set_power(void) {
    sysctl_set_power_mode(SYSCTL_POWER_BANK6, SYSCTL_POWER_V18);
    sysctl_set_power_mode(SYSCTL_POWER_BANK7, SYSCTL_POWER_V18);
}

static void lcd_io_init(void) {
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

#define UART_NUM    UART_DEVICE_3
static void uart_io_init(void) {
    fpioa_set_function(4, FUNC_UART1_RX + UART_NUM * 2);
    fpioa_set_function(5, FUNC_UART1_TX + UART_NUM * 2);
    fpioa_set_function(10, FUNC_GPIOHS3);
}

typedef int (*command_entry_t)(void * ptr, int argc, char ** argv); 
struct command {
    const char * name;
    void * ptr;
    command_entry_t entry;
};

static int lcd_ptr_y;
static bool lcd_inited;

int clear_screen(void * ptr, int argc, char ** argv) {
    lcd_ptr_y = 0;
    lcd_clear(BLACK);
    return 0;
}

void lcd_putchar(char c) {
    static char line_buffer[40];
    static int line_ptr;
    if(c == '\n' || line_ptr == 40) {
        line_buffer[line_ptr] = 0;
        lcd_set_area(0, lcd_ptr_y * 20, LCD_Y_MAX, (lcd_ptr_y + 1) * 20);
        uint32_t data = ((uint32_t)BLACK << 16) | (uint32_t)BLACK;
        tft_fill_data(&data, 20 * LCD_Y_MAX / 2);
        // tft_fill_data(uint32_t *data_buf, uint32_t length)
        // lcd_clear(BLACK);
        lcd_set_area(0, 0, LCD_Y_MAX, LCD_X_MAX);
        lcd_draw_string(0, lcd_ptr_y * 20, line_buffer, WHITE);
        line_ptr = 0;
        lcd_ptr_y++;
        if(lcd_ptr_y * 20 >= LCD_X_MAX) {
            lcd_ptr_y = 0;
        }
        if(c != '\n') {
            line_buffer[line_ptr++] = '~';
            line_buffer[line_ptr++] = c;
        }
    } else {
        line_buffer[line_ptr++] = c;
    }
}
void lcd_puts(const char *s) {
    for(; *s; s++) {
        lcd_putchar(*s);
    }
}

void combine_putchar(char c) {
    extern volatile uart_t *const uart[3];
    while(uart[UART_DEVICE_3]->LSR & (1u << 5))
        continue;
    uart[UART_DEVICE_3]->THR = c;
    if(lcd_inited) {
        lcd_putchar(c);
    }
}

uint8_t * make_buf(int n, char * r, int * len) {
    int w = strlen(r);
    char * buf = malloc(n * w + 1);
    for(int i = 0; i < n; i++) {
        strncpy(buf + i * w, r, w);
    }
    buf[n * w] = 0;
    *len = n * w;
    return (uint8_t*) buf;
}

int handle_mysha(void * ptr, int argc, char ** argv) {
    uint8_t output[SHA256_HASH_LEN];
    SHA256_CTX ctx;
    if(argc >= 3) {
        int n;
        uint8_t * buf = make_buf(atoi(argv[1]), argv[2], &n);
        long start = read_cycle();
        my_sha256_init(&ctx);
        my_sha256_update(&ctx, (uint8_t*)argv[1], n);
        my_sha256_final(&ctx, output);
        long end = read_cycle();
        for(size_t i = 0; i < SHA256_HASH_LEN; i++) {
            printf("%02x", output[i]);
        }
        printf("\ncycles=%ld\n", end - start);
        free(buf);
        return 0;
    }
    else if(argc >= 2) {
        long start = read_cycle();
        my_sha256_init(&ctx);
        my_sha256_update(&ctx, (uint8_t*)argv[1], strlen(argv[1]));
        my_sha256_final(&ctx, output);
        long end = read_cycle();
        for(size_t i = 0; i < SHA256_HASH_LEN; i++) {
            printf("%02x", output[i]);
        }
        printf("\ncycles=%ld\n", end - start);
        return 0;
    }
    return 1;
}

int handle_sha(void * ptr, int argc, char ** argv) {
    uint8_t output[SHA256_HASH_LEN];
    SHA256_CTX ctx;
    if(argc >= 3) {
        int n;
        uint8_t * buf = make_buf(atoi(argv[1]), argv[2], &n);
        long start = read_cycle();
        sha256_hard_calculate((uint8_t*)buf, n, output);
        long end = read_cycle();
        for(size_t i = 0; i < SHA256_HASH_LEN; i++) {
            printf("%02x", output[i]);
        }
        printf("\ncycles=%ld\n", end - start);
        free(buf);
        return 0;
    }
    else if(argc >= 2) {
        long start = read_cycle();
        sha256_hard_calculate((uint8_t*)argv[1], strlen(argv[1]), output);
        long end = read_cycle();
        for(size_t i = 0; i < SHA256_HASH_LEN; i++) {
            printf("%02x", output[i]);
        }
        printf("\ncycles=%ld\n", end - start);
        return 0;
    }
    printf("Usage: sha <data>\n");
    return 1;
}

int handle_lcd(void * ptr, int argc, char ** argv) {
    static const char *color_name[] = {"red", "green", "blue", "black", "white", NULL};
    static const uint16_t color[] = {RED, GREEN, BLUE, BLACK, WHITE};
    if(argc >= 2 && strcmp(argv[1], "clear") == 0) {
        if(argc >= 3) {
            bool found = false;
            for(int i = 0; color_name[i]; i++) {
                if(strcmp(color_name[i], argv[2]) == 0) {
                    found = true;
                    printf("clear to color %s\n", color_name[i]);
                    lcd_clear(color[i]);
                }
            }
            if(!found) {
                printf("Invalid color name: %s\n", argv[2]);
                printf("Candidate: ");
                for(int i = 0; color_name[i]; i++) {
                    printf(" %s", color_name[i]);
                }
                printf("\n");
                return 1;
            }
        } else {
            printf("clear to color %s\n", "black");
            lcd_clear(BLACK);
        }
        return 0;
    }
    else if(argc >= 2 && strcmp(argv[1], "init") == 0) {
        printf("init lcd\n");
        lcd_io_set_power();
        lcd_init();
        lcd_set_direction(DIR_YX_RLDU);
        lcd_clear(BLACK);
        lcd_inited = true;
        printf("lcd init finish\n");
        return 0;
    }
    printf("Usage: %s reset [color]  clear the lcd\n", argv[0]);
    printf("       %s init           init the lcd\n", argv[0]);
    return 1;
}

struct command cmd[] = {
    {"sha", NULL, handle_sha},
    {"sha_cpu", NULL, handle_mysha},
    {"lcd", NULL, handle_lcd},
    {"clear", NULL, clear_screen},
    {0, 0}
};

void handle_command(char buffer[]) {
    char * argv[128] = {};
    int argc = 0;
    for(size_t i = 0; buffer[i]; i++) {
        if(buffer[i] == ' ' || buffer[i] == '\r' || buffer[i] == '\t') {
            buffer[i] = 0;
        }
        if(buffer[i] && (i == 0 || buffer[i - 1] == 0)) {
            argv[argc++] = buffer + i;
        }
    }
    // for(int i = 0; i < argc; i++) {
    //     printf("argv[%d] = %s\n", i, argv[i]);
    // }
    if(argc == 0) return;
    for(size_t i = 0; cmd[i].name; i++) {
        if(strcmp(cmd[i].name, argv[0]) == 0) {
            int res = cmd[i].entry(cmd[i].ptr, argc, argv);
            if(res != 0) {
                printf("\nCommand Fail: %d\n", res);
            }
            return;
        }
    }
    printf("Unknown command: %s\n", argv[0]);
    return;
}

int main(void) {
    uart_io_init();
    lcd_io_init();
    char line_buffer[1024];
    static const char prompt[] = "> ";
    sys_register_putchar(combine_putchar);
    uart_send_data(UART_NUM, prompt, strlen(prompt));
    lcd_puts(prompt);
    while (1) {
        gets(line_buffer);
        handle_command(line_buffer);
        uart_send_data(UART_NUM, prompt, strlen(prompt));
        lcd_puts(prompt);
    }
    return 0;
}
