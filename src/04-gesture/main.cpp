#include "atomic.h"
#include "board_config.h"
#include "dmac.h"
#include "entry.h"
#include "gpio_common.h"
#include "gsl/gsl-lite.hpp"
#include "kpu.h"
#include "plic.h"
#include "sleep.h"
#include "stdio.h"
#include "sysctl.h"
#include "gpiohs.h"
#include "iomem.h"
#include "tick.h"
#include "plic.h"
#include "gc0328.h"
#include "fpioa.h"
#include "iomem.h"
#include "key.h"
#include "lcd.h"
#include "dvp.h"
#include "uarths.h"
#include "pwm.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <math.h>
#include <nncase/runtime/datatypes.h>
#include <nncase/runtime/runtime_op_utility.h>
#include <nncase/runtime/runtime_tensor.h>
#include <nncase/runtime/interpreter.h>
#include <nncase/runtime/result.h>
using namespace nncase;
using namespace nncase::runtime;

extern "C" {
__attribute__((weak)) void* __dso_handle;
}


#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#define INCBIN_PREFIX
#include "incbin.h"
INCBIN(model, "hagrid2.kmodel");
// INCBIN(input, "hagrid2.bin");

#define HAGRID

#if defined(COCOS)
static const char * names[] = {
    "person",
    "bicycle",
    "car",
    "motorcycle",
    "airplane",
    "bus",
    "train",
    "truck",
    "boat",
    "traffic light",
    "fire hydrant",
    "stop sign",
    "parking meter",
    "bench",
    "bird",
    "cat",
    "dog",
    "horse",
    "sheep",
    "cow",
    "elephant",
    "bear",
    "zebra",
    "giraffe",
    "backpack",
    "umbrella",
    "handbag",
    "tie",
    "suitcase",
    "frisbee",
    "skis",
    "snowboard",
    "sports ball",
    "kite",
    "baseball bat",
    "baseball glove",
    "skateboard",
    "surfboard",
    "tennis racket",
    "bottle",
    "wine glass",
    "cup",
    "fork",
    "knife",
    "spoon",
    "bowl",
    "banana",
    "apple",
    "sandwich",
    "orange",
    "broccoli",
    "carrot",
    "hot dog",
    "pizza",
    "donut",
    "cake",
    "chair",
    "couch",
    "potted plant",
    "bed",
    "dining table",
    "toilet",
    "tv",
    "laptop",
    "mouse",
    "remote",
    "keyboard",
    "cell phone",
    "microwave",
    "oven",
    "toaster",
    "sink",
    "refrigerator",
    "book",
    "clock",
    "vase",
    "scissors",
    "teddy bear",
    "hair drier",
    "toothbrush",
};
#elif defined(HAGRID)
static const char * names[] = {
  "grabbing",
  "grip",
  "holy",
  "point",
  "call",
  "three3",
  "timeout",
  "xsign",
  "hand_heart",
  "hand_heart2",
  "little_finger",
  "middle_finger",
  "take_picture",
  "dislike",
  "fist",
  "four",
  "like",
  "mute",
  "ok",
  "one",
  "palm",
  "peace",
  "peace_inverted",
  "rock",
  "stop",
  "stop_inverted",
  "three",
  "three2",
  "two_up",
  "two_up_inverted",
  "three_gun",
  "thumb_index",
  "thumb_index2",
  "no_gesture",
};
#endif

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

volatile uint32_t g_ai_done_flag;
static void ai_done(void *ctx) {
    g_ai_done_flag = 1;
}

kpu_model_context_t model;
struct color_t {
    uint16_t b: 5;
    uint16_t g: 6;
    uint16_t r: 5;
    inline operator uint16_t() const {
        return *(uint16_t*)this;
    }
};

static inline color_t make_color(uint8_t r, uint8_t g, uint8_t b) {
    return (color_t) {
        .b = uint16_t(b / 8),
        .g = uint16_t(g / 4),
        .r = uint16_t(r / 8)
    };
}
static inline uint16_t color_to_u16(color_t color) {
    return *(uint16_t*)(&color);
}

struct io_buf_t {
    uint8_t img[3][256][320];
    color_t lcd[240][320];
    uint8_t img_buffer[3][256][320];
};

static io_buf_t * io_buf;

static int clamp(int x, int mn, int mx) {
    if(x < mn) return mn;
    if(x > mx) return mx;
    return x;
}

static color_t cubehelixColor(float t, float start = 0.5, float rotations = -1.5, float hue = 1.2, float gamma = 1.0) {
    // 调整亮度值 (gamma 校正)
    t = std::pow(t, gamma);

    // 计算色调角度
    float angle = 2.0 * M_PI * (start / 3.0 + rotations * t);

    // 定义系数
    float amp = hue * t * (1.0 - t) / 2.0;
    float cosA = std::cos(angle);
    float sinA = std::sin(angle);

    // 计算 RGB
    float r = t + amp * (-0.14861 * cosA + 1.78277 * sinA);
    float g = t + amp * (-0.29227 * cosA - 0.90649 * sinA);
    float b = t + amp * (+1.97294 * cosA);

    return {
        static_cast<uint16_t>(clamp(b, 0.0, 1.0) * 31), // 蓝色 (5位)
        static_cast<uint16_t>(clamp(g, 0.0, 1.0) * 63), // 绿色 (6位)
        static_cast<uint16_t>(clamp(r, 0.0, 1.0) * 31)  // 红色 (5位)
    };
}
struct box_t {
    int x1, y1, x2, y2;
    const char * name;
    color_t color;
    float proba;
};
static box_t targets[420];
static int num_targets;
static corelock_t target_lock;
static corelock_t image_lock;
static color_t line_buf[16*8*128];

static void refresh_lcd() {
    char buf[128];
    corelock_lock(&target_lock);
    for(int i = 0; i < num_targets; i++) {
        int x1 = targets[i].x1;
        int y1 = targets[i].y1;
        int x2 = targets[i].x2;
        int y2 = targets[i].y2;
        color_t color = targets[i].color;
        for(int i = x1; i <= x2; i++) {
            io_buf->lcd[y1][i^1] = color;
            io_buf->lcd[y2][i^1] = color;
        }
        for(int i = y1; i <= y2; i++) {
            io_buf->lcd[i][x1^1] = color;
            io_buf->lcd[i][x2^1] = color;
        }
        sprintf(buf, "%s:%.2f", targets[i].name, targets[i].proba);
        uint16_t font_color;
        if(color.r * 2 + color.g + color.b * 2 > 96) {
            font_color = 0x0000;
        } else {
            font_color = 0xffff;
        }
        lcd_ram_draw_string(buf, (uint32_t*)line_buf, font_color, color);
        int width = strlen(buf) * 8, height = 16;
        color_t (*lb)[width];
        *(color_t**)(&lb) = line_buf;
        for(int i = 0; i < height; i++) {
            for(int j = 0; j < width; j++) {
                if(y1 + i < 240 && x1 + j < 320) {
                    io_buf->lcd[y1 + i][x1 + j] = lb[i][j];
                }
            }
        }
    }
    corelock_unlock(&target_lock);
    lcd_draw_picture(0, 0, 320, 240, (uint32_t*)&io_buf->lcd);
}

static volatile bool g_dvp_finish_flag = false;
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

static void dvp_thread_init() {
    dvp_init(8);
    dvp_set_xclk_rate(24000000);
    dvp_enable_burst();
    dvp_set_output_enable(DVP_OUTPUT_AI, 1);
    dvp_set_output_enable(DVP_OUTPUT_DISPLAY, 1);
    dvp_set_image_format(DVP_CFG_RGB_FORMAT);
    dvp_set_image_size(320, 240);
    gc0328_init();
    open_gc0328_1();
    dvp_set_ai_addr(
        (unsigned long)&io_buf->img[0],
        (unsigned long)&io_buf->img[1], 
        (unsigned long)&io_buf->img[2]
    );
    dvp_set_display_addr((long)&io_buf->lcd);
    dvp_config_interrupt(DVP_CFG_START_INT_ENABLE | DVP_CFG_FINISH_INT_ENABLE, 0);
    dvp_disable_auto();
}

static int dvp_thread_main(void *ctx) {
    corelock_lock(&image_lock);
    while(true) {
        dvp_clear_interrupt(DVP_STS_FRAME_START | DVP_STS_FRAME_FINISH);
        dvp_config_interrupt(DVP_CFG_START_INT_ENABLE | DVP_CFG_FINISH_INT_ENABLE, 1);
        while(!g_dvp_finish_flag);
        corelock_unlock(&image_lock);
        refresh_lcd();
        corelock_lock(&image_lock);
        g_dvp_finish_flag = 0;
    }
    return 0;
}

static int debug_dvp_thread_main(void * ctx) {
    printf("start lcd thread\n");
    while(true) {
        refresh_lcd();
        usleep(100*1000);
    }
    return 0;
}

static int overlap(int x1, int x2, int y1, int y2) {
    int l = std::max(x1, y1);
    int r = std::min(x2, y2);
    return std::max(r - l, 0);
}
struct box {
    int x1, y1, x2, y2;
    int id;
    float proba;
};
static int box_inter(const box & a, const box & b) {
    return overlap(a.x1, a.x2, b.x1, b.x2) * overlap(a.y1, a.y2, b.y1, b.y2);
}
static float box_iou(const box & a, const box & b) {
    int size_a = (a.x2 - a.x1) * (a.y2 - a.y1);
    int size_b = (b.x2 - b.x1) * (b.y2 - b.y1);
    int inter = box_inter(a, b);
    return 1.0 * inter / (size_a + size_b - inter); 
}
static int yolo_nms(int n_class, int n_box, float *_data, box *boxes, float th_proba, float th_iou) {
    float (*data)[n_box];
    *(float**)&data = _data;
    int res_box = 0;
    for(int j = 0; j < n_box; j++) {
        float mx = data[4][j];
        int mi = 0;
        for(int i = 0; i < n_class; i++) {
            if(data[i + 4][j] > mx) {
                mi = i;
                mx = data[i + 4][j];
            }
        }
        if(mx > th_proba) {
            boxes[res_box++] = (box) {
                .x1 = int(data[0][j] - data[2][j] / 2),
                .y1 = int(data[1][j] - data[3][j] / 2),
                .x2 = int(data[0][j] + data[2][j] / 2),
                .y2 = int(data[1][j] + data[3][j] / 2),
                .id = mi,
                .proba = mx
            };
        }
    }
    std::sort(boxes, boxes+res_box, [](const box&a, const box &b) {
        return a.proba > b.proba;
    });
    for(int i = 0; i < res_box; i++) {
        for(int j = i + 1; j < res_box; j++) {
            if(box_iou(boxes[j], boxes[i]) > th_iou) {
                boxes[j].proba = 0;
            }
        }
    }
    int new_cnt = 0;
    for(int i = 0; i < res_box; i++) {
        if(boxes[i].proba != 0) {
            boxes[new_cnt++] = boxes[i];
        }
    }
    return new_cnt;
}

// inline const bool debug_mode = false;
static int ai_thread_main(void * ctx) {
    interpreter interp;
    interp.load_model({(gsl::byte*)model_data, SIZE_MAX}).unwrap();
    interp.options().set("dma_ch", (uint32_t)DMAC_CHANNEL5).unwrap();
    printf("create input 0\n");
    printf("create output 0\n");
    printf("set input 0\n");
    auto shape = interp.input_shape(0);
    const int C = shape[0], H = shape[1], W = shape[2];
    const int sx = (240 + H - 1) / H, sy = (320 + W - 1) / W;
    const int nx = 240 / sx, ny = 320 / sy;
    printf("C=%d H=%d W=%d\n", C, H, W);
    printf("sx=%d sy=%d nx=%d ny=%d\n", sx, sy, nx, ny);
    auto input_tensor = hrt::create(datatype_t::dt_uint8, shape, {(gsl::byte*)io_buf->img_buffer, get_bytes(datatype_t::dt_uint8, shape)}, false).unwrap();
    interp.input_tensor(0, input_tensor).unwrap();
    // corelock_lock(&image_lock);
    while(true) {
        printf("sync image\n");
        uint8_t (*img_in)[H][W];
        *(uint8_t**)(&img_in) = (uint8_t*)io_buf->img_buffer;
        printf("clear\n");
        printf("write img\n");
        for(int c = 0; c < 3; c++) {
            for(int i = 0; i < nx; i++) {
                for(int j = 0; j < ny; j++) {
                    img_in[c][i][j] = io_buf->img[c][i * sx][j * sy];
                }
            }
        }
        // corelock_unlock(&image_lock);
        hrt::sync(input_tensor, host_runtime_tensor::sync_write_back).unwrap();
        printf("run model\n");
        interp.run().unwrap();
        auto output = hrt::map(interp.output_tensor(0).unwrap(), host_runtime_tensor::map_read).unwrap().buffer();
        const int shape0 = interp.output_shape(0)[0];
        const int shape1 = interp.output_shape(0)[1];
        box boxes[shape1];
        int n_boxes = yolo_nms(shape0 - 4, shape1, (float*)output.data(), boxes, 0.1, 0.25);
        // corelock_lock(&image_lock);
        printf("wait lock\n");
        corelock_lock(&target_lock);
        num_targets = n_boxes;
        for(int i = 0; i < n_boxes; i++) {
            targets[i] = (box_t) {
                .x1=clamp(boxes[i].x1 * sx, 0, 320-1),
                .y1=clamp(boxes[i].y1 * sy, 0, 240-1),
                .x2=clamp(boxes[i].x2 * sx, 0, 320-1),
                .y2=clamp(boxes[i].y2 * sy, 0, 240-1),
                .name=names[boxes[i].id],
                .color=cubehelixColor(1.0 * boxes[i].id / (shape0 - 4)),
                .proba=boxes[i].proba
            };
        }
        corelock_unlock(&target_lock);
    }
}

extern "C" int main() {
    sysctl_pll_set_freq(SYSCTL_PLL0, 800000000UL);
    sysctl_pll_set_freq(SYSCTL_PLL1, 400000000UL);
    sysctl_pll_set_freq(SYSCTL_PLL2, 45158400UL);
    sysctl_clock_enable(SYSCTL_CLOCK_AI);

    uarths_init();
    plic_init();
    
    io_init();
    io_set_power();

    io_buf = (io_buf_t*)iomem_malloc(sizeof(io_buf_t));

    lcd_init();
    lcd_set_direction(DIR_YX_LRUD);
    lcd_clear(BLACK);

    // if(!debug_mode) {
    dvp_thread_init();
    // }

    plic_set_priority(IRQN_DVP_INTERRUPT, 1);
    plic_irq_register(IRQN_DVP_INTERRUPT, on_irq_dvp, NULL);
    plic_irq_enable(IRQN_DVP_INTERRUPT);

    tick_init(TICK_NANOSECONDS);
    sysctl_enable_irq();

    // if(debug_mode) {
    //     // memcpy(io_buf->img_in, input_data, sizeof(io_buf->img_in));
    //     uint8_t (*input_data_)[256][320] = (uint8_t(*)[256][320])input_data;
    //     for(int i = 0; i < 240; i++) {
    //         for(int j = 0; j < 320; j++) {
    //             io_buf->lcd[i][j^1] = make_color(
    //                 input_data_[0][i][j], 
    //                 input_data_[1][i][j], 
    //                 input_data_[2][i][j]
    //             );
    //             for(int c = 0; c < 3; c++) {
    //                 io_buf->img[c][i][j] = input_data_[c][i][j];
    //             }
    //         }
    //     }
    //     register_core1(debug_dvp_thread_main, NULL);
    // } else {
    register_core1(dvp_thread_main, NULL);
    // }
    ai_thread_main(NULL);
    return 0;
}
