#ifndef P10_DISPLAY_H
#define P10_DISPLAY_H

#include <stdint.h>

typedef enum {
    ALIGN_LEFT,
    ALIGN_CENTER,
    ALIGN_RIGHT
} p10_align_h_t;

typedef enum {
    ALIGN_TOP,
    ALIGN_MIDDLE,
    ALIGN_BOTTOM
} p10_align_v_t;

void p10_init(void);
void p10_clear(void);
void p10_draw_logo(void);

void p10_string_to_buffer(const char *str, int x, int y, p10_align_h_t align_h, p10_align_v_t align_v);
void p10_int_to_buffer(int angka, int x, int y, p10_align_h_t align_h, p10_align_v_t align_v);
void p10_timestamp_to_buffer(long timestamp, int x, int y, p10_align_h_t align_h, p10_align_v_t align_v);

// ← ini yang kurang!
void p10_set_brightness(uint8_t persentase);

#endif // P10_DISPLAY_H