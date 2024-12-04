#include "kpu.h"
#include "sleep.h"
#include "stdio.h"
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#define INCBIN_PREFIX
#include "incbin.h"
INCBIN(model, "mnist.kmodel");
INCBIN(infer, "infer.bin");
kpu_model_context_t mnist;

volatile uint32_t g_ai_done_flag;
static void ai_done(void *ctx) {
    g_ai_done_flag = 1;
}

int main(void) {
    if(kpu_load_kmodel(&mnist, model_data) != 0) {
        printf("Unable to load model\n");
        while(1);
    }
    for(int i = 0; i < 100; i++) {
        g_ai_done_flag = 0;
        kpu_run_kmodel(&mnist, &infer_data[i * 28 * 28], DMAC_CHANNEL5, ai_done, NULL);
        while(!g_ai_done_flag);
        float * output;
	    size_t output_size;
        kpu_get_output(&mnist, 0, &output, &output_size);
        float max_value = output[0];
        int max_pos = 0;
        for(int j = 0; j < 10; j++) {
            if(output[j] > max_value) {
                max_value = output[j];
                max_pos = j;
            }
        }
        printf("max_pos=%d\n", max_pos);
        msleep(100);
    }
    return 0;
}
