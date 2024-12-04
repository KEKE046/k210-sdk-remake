#include "kpu.h"
#include "sleep.h"
#include "stdio.h"
#include "sysctl.h"
#include "uarths.h"
#include "w25qxx.h"
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#define INCBIN_PREFIX
#include "incbin.h"
INCBIN(model, "final.kmodel");
INCBIN(infer, "infer.bin");
kpu_model_context_t mnist;

volatile uint32_t g_ai_done_flag;
static void ai_done(void *ctx) {
    g_ai_done_flag = 1;
}

int main(void) {
    // sysctl_pll_set_freq(SYSCTL_PLL0, 800000000UL);
    // sysctl_pll_set_freq(SYSCTL_PLL1, 400000000UL);
    sysctl_clock_enable(SYSCTL_CLOCK_AI);
    // uarths_init();
    plic_init();
    printf("Loading model %s\n", model_data);
    if(kpu_load_kmodel(&mnist, model_data) != 0) {
        printf("Unable to load model\n");
        while(1);
    }
    sysctl_enable_irq();
    printf("Load model finish\n");
    for(int i = 0; i < 100; i++) {
        printf("Run example %d\n", i);
        printf("[01] Send data to model\n");
        g_ai_done_flag = 0;
        kpu_run_kmodel(&mnist, &infer_data[i * 28 * 28], DMAC_CHANNEL5, ai_done, NULL);
        printf("[02] Waiting finish\n");
        while(!g_ai_done_flag);
        float * output;
	    size_t output_size;
        printf("[03] Get output\n");
        kpu_get_output(&mnist, 0, &output, &output_size);
        float max_value = output[0];
        int max_pos = 0;
        for(int j = 0; j < 10; j++) {
            if(output[j] > max_value) {
                max_value = output[j];
                max_pos = j;
            }
        }
        printf("[04] Result: max_pos=%d\n", max_pos);
        msleep(1000);
    }
    return 0;
}
