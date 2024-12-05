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
#include <nncase/runtime/datatypes.h>
#include <nncase/runtime/result.h>
#include <nncase/runtime/runtime_op_utility.h>
#include <nncase/runtime/runtime_tensor.h>
#include <stdio.h>
#include <string.h>
#include "dmac.h"
#include "dvp.h"
#include "fpioa.h"
#include "gsl/gsl-lite.hpp"
#include "lcd.h"
#include "plic.h"
#include "sysctl.h"
#include "uarths.h"
#include "board_config.h"
#include "gpiohs.h"
#include "iomem.h"
#include "nncase/runtime/interpreter.h"
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#define INCBIN_PREFIX
#include "incbin.h"
INCBIN(dummy_model, "ddream.kmodel");

using namespace nncase;
using namespace nncase::runtime;

#if (BOARD_VERSION == BOARD_V1_2_LE)
#include "ov2640.h"
#elif (BOARD_VERSION == BOARD_V1_3)
#include "key.h"
#include "pwm.h"
#include "gc0328.h"
#include "tick.h"
#endif

volatile uint8_t g_dvp_finish_flag;
extern "C" {
    __attribute__((weak)) void* __dso_handle;
}

static int on_irq_dvp(void* ctx) {
    if (dvp_get_interrupt(DVP_STS_FRAME_FINISH)) {
        dvp_config_interrupt(DVP_CFG_START_INT_ENABLE | DVP_CFG_FINISH_INT_ENABLE, 0);
        dvp_clear_interrupt(DVP_STS_FRAME_FINISH);
        g_dvp_finish_flag = 1;
    }
    else {
        if(g_dvp_finish_flag == 0)
            dvp_start_convert();
        dvp_clear_interrupt(DVP_STS_FRAME_START);
    }
    return 0;
}

static void io_init(void) {
    /* Init DVP IO map and function settings */
#if (BOARD_VERSION == BOARD_V1_2_LE)
    fpioa_set_function(DVP_RST_PIN, FUNC_CMOS_RST);
#endif
    fpioa_set_function(DVP_PWDN_PIN, FUNC_CMOS_PWDN);
    fpioa_set_function(DVP_XCLK_PIN, FUNC_CMOS_XCLK);
    fpioa_set_function(DVP_VSYNC_PIN, FUNC_CMOS_VSYNC);
    fpioa_set_function(DVP_HREF_PIN, FUNC_CMOS_HREF);
    fpioa_set_function(DVP_PCLK_PIN, FUNC_CMOS_PCLK);
    fpioa_set_function(DVP_SCCB_SCLK_PIN, FUNC_SCCB_SCLK);
    fpioa_set_function(DVP_SCCB_SDA_PIN, FUNC_SCCB_SDA);

    /* Init SPI IO map and function settings */
    fpioa_set_function(LCD_DC_PIN, fpioa_function_t(FUNC_GPIOHS0 + LCD_DC_IO));
    fpioa_set_function(LCD_CS_PIN, FUNC_SPI0_SS3);
    fpioa_set_function(LCD_RW_PIN, FUNC_SPI0_SCLK);
    fpioa_set_function(LCD_RST_PIN, fpioa_function_t(FUNC_GPIOHS0 + LCD_RST_IO));

    sysctl_set_spi0_dvp_data(1);

    /* LCD Backlight */
    fpioa_set_function(LCD_BLIGHT_PIN, fpioa_function_t(FUNC_GPIOHS0 + LCD_BLIGHT_IO));
    gpiohs_set_drive_mode(LCD_BLIGHT_IO, GPIO_DM_OUTPUT);
    gpiohs_set_pin(LCD_BLIGHT_IO, GPIO_PV_LOW);

#if (BOARD_VERSION == BOARD_V1_3)
    /* KEY IO map and function settings */
    fpioa_set_function(KEY_PIN, fpioa_function_t(FUNC_GPIOHS0 + KEY_IO));
    gpiohs_set_drive_mode(KEY_IO, GPIO_DM_INPUT_PULL_UP);
    gpiohs_set_pin_edge(KEY_IO, GPIO_PE_FALLING);
    gpiohs_irq_register(KEY_IO, 1, key_trigger, NULL);

     /* pwm IO.*/
    fpioa_set_function(LED_IR_PIN, fpioa_function_t(FUNC_TIMER0_TOGGLE2 + 1 * 4));         
     /* Init PWM */
    pwm_init(PWM_DEVICE_1);
    pwm_set_frequency(PWM_DEVICE_1, PWM_CHANNEL_1, 3000, 0.3); 
    pwm_set_enable(PWM_DEVICE_1, PWM_CHANNEL_1, 0);
#endif
}

static void io_set_power(void) {
    /* Set dvp and spi pin to 1.8V */
    sysctl_set_power_mode(SYSCTL_POWER_BANK6, SYSCTL_POWER_V18);
    sysctl_set_power_mode(SYSCTL_POWER_BANK7, SYSCTL_POWER_V18);
}

struct color_t {
    uint16_t b: 5;
    uint16_t g: 6;
    uint16_t r: 5;
};
static_assert(sizeof(color_t) == sizeof(uint16_t), "color_t should equal to u16");

struct ddream_t {
    inline static const int C = 3;
    inline static const int W = 64;
    inline static const int H = 64;
    struct io_buf_t {
        uint8_t img[C][W][H];
        uint8_t img_buf[2][C][W][H];
        float lr;
        color_t lcd[W][H];
    };
    io_buf_t * io_buf;
    interpreter interp;
    runtime_tensor img;
    runtime_tensor img_buf[2];
    uint8_t cur_buf = 0;
    runtime_tensor lr;
    static result<runtime_tensor> create_tensor(datatype_t dt, runtime_shape_t shape, void * ptr) {
        try_var(tensor, hrt::create(dt, shape, gsl::span<gsl::byte>((gsl::byte*)ptr, get_bytes(dt, shape)), false));
        return ok(tensor);
    }
    result<void> init(void * model_data) {
        io_buf = (io_buf_t*)iomem_malloc(sizeof(io_buf_t));
        datatype_t u8 = datatype_t::dt_uint8;
        runtime_shape_t patch_shape = {C, W, H};
        try_set(img, create_tensor(u8, patch_shape, &io_buf->img));
        try_set(img_buf[0], create_tensor(u8, patch_shape, &io_buf->img_buf[0]));
        try_set(img_buf[1], create_tensor(u8, patch_shape, &io_buf->img_buf[1]));
        try_set(lr, create_tensor(datatype_t::dt_float32, {1}, &io_buf->lr));
        printf("Load model\n");
        try_(interp.load_model(gsl::span<gsl::byte>((gsl::byte*)model_data, SIZE_MAX)));
        for(int i = 0, e = interp.inputs_size(); i < e; i++) {
            printf("  input [%d] = [", i);
            runtime_shape_t shape = interp.input_shape(i);
            for(int j = 0; j < shape.size(); j++) {
                if(j) printf(", ");
                printf("%ld", shape[j]);
            }
            printf("]\n");
        }
        for(int i = 0, e = interp.outputs_size(); i < e; i++) {
            printf("  output[%d] = [", i);
            runtime_shape_t shape = interp.output_shape(i);
            for(int j = 0; j < shape.size(); j++) {
                if(j) printf(", ");
                printf("%ld", shape[j]);
            }
            printf("]\n");
        }
        try_(interp.options().set("dma_ch", (uint32_t)DMAC_CHANNEL5));
        return ok();
    }
    void set_lr(float lr) {
        io_buf->lr = lr;
    }
    result<void> run(int times) {
        // printf("Set input  0 <= img\n");
        try_(interp.input_tensor(0, img));
        // printf("Set input  1 <= %f\n", io_buf->lr);
        try_(interp.input_tensor(1, lr));
        // printf("Set output 0 <= img_buf[%d]\n", cur_buf);
        try_(interp.output_tensor(0, img_buf[cur_buf]));
        // printf("run (%d)\n", 0);
        try_(interp.run());
        for(int i = 1; i < times; i++) {
            // printf("Set input 0 <= img_buf[%d]\n", cur_buf);
            try_(interp.input_tensor(0, img_buf[cur_buf]));
            try_(interp.input_tensor(1, lr));
            // printf("Set output 0 <= img_buf[%d]\n", cur_buf);
            try_(interp.output_tensor(0, img_buf[cur_buf ^ 1]));
            // printf("run (%d)\n", i);
            try_(interp.run());
            cur_buf ^= 1;
        }
        return ok();
    }
    void sync_lcd() {
        for(int i = 0; i < W; i++) {
            for(int j = 0; j < H; j+=2) {
                {
                    uint8_t r = io_buf->img_buf[cur_buf][0][i][j] / 8;
                    uint8_t g = io_buf->img_buf[cur_buf][1][i][j] / 4;
                    uint8_t b = io_buf->img_buf[cur_buf][2][i][j] / 8;
                    io_buf->lcd[i][j+1] = {
                        .b=b, .g=g, .r=r
                    };
                }
                {
                    uint8_t r = io_buf->img_buf[cur_buf][0][i][j+1] / 8;
                    uint8_t g = io_buf->img_buf[cur_buf][1][i][j+1] / 4;
                    uint8_t b = io_buf->img_buf[cur_buf][2][i][j+1] / 8;
                    io_buf->lcd[i][j] = {
                        .b=b, .g=g, .r=r
                    };
                }
            }
        }
        lcd_draw_picture(0, 0, W, H, (uint32_t*)io_buf->lcd);
    }
};

static ddream_t ddream;

extern "C"
int main(void) {
    sysctl_pll_set_freq(SYSCTL_PLL0, 800000000UL);
    sysctl_pll_set_freq(SYSCTL_PLL1, 400000000UL);
    sysctl_pll_set_freq(SYSCTL_PLL2, 45158400UL);
    uarths_init();

    plic_init();
    io_init();
    io_set_power();

    ddream.init(dummy_model_data).unwrap();
    ddream.set_lr(0.15);

    printf("LCD init\n");
    lcd_init();
    lcd_set_direction(DIR_YX_RLDU);
    lcd_clear(BLACK);

    printf("DVP init\n");
    dvp_init(8);
    dvp_set_xclk_rate(24000000);
    dvp_enable_burst();
    dvp_set_output_enable(DVP_OUTPUT_AI, 1);
    dvp_set_image_format(DVP_CFG_RGB_FORMAT);

    dvp_set_image_size(64, 64);
    gc0328_init();
    open_gc0328_1();

    dvp_set_ai_addr(
        (unsigned long)&ddream.io_buf->img[0], 
        (unsigned long)&ddream.io_buf->img[1],
        (unsigned long)&ddream.io_buf->img[2]
    );
    // dvp_set_display_addr((uint32_t)g_lcd_gram);
    dvp_config_interrupt(DVP_CFG_START_INT_ENABLE | DVP_CFG_FINISH_INT_ENABLE, 0);
    dvp_disable_auto();

    /* DVP interrupt config */
    printf("DVP interrupt config\n");
    plic_set_priority(IRQN_DVP_INTERRUPT, 1);
    plic_irq_register(IRQN_DVP_INTERRUPT, on_irq_dvp, NULL);
    plic_irq_enable(IRQN_DVP_INTERRUPT);

    sysctl_enable_irq();

    printf("system start\n");
    g_dvp_finish_flag = 0;
#if (BOARD_VERSION == BOARD_V1_3)
    tick_init(TICK_NANOSECONDS);
#endif

    while (1) {
        if (KEY_PRESS == key_get()) {
            // camera_switch();
        }

        dvp_clear_interrupt(DVP_STS_FRAME_START | DVP_STS_FRAME_FINISH);
        dvp_config_interrupt(DVP_CFG_START_INT_ENABLE | DVP_CFG_FINISH_INT_ENABLE, 1);

        while(!g_dvp_finish_flag);
        g_dvp_finish_flag = 0;
        printf("run model\n");
        ddream.run(1).unwrap();
        printf("sync\n");
        ddream.sync_lcd();
    }
    return 0;
}
