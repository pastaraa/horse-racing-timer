#ifndef P10_DISPLAY_H
#define P10_DISPLAY_H

#include <stdint.h>

// Tipe perataan teks (Alignment) seperti yang bos kamu minta
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

// Fungsi-fungsi utama P10
void p10_init(void);
void p10_clear(void);
void p10_draw_logo(void);

// Fungsi Teks & Angka
void p10_string_to_buffer(const char *str, int x, int y, p10_align_h_t align_h, p10_align_v_t align_v);
void p10_int_to_buffer(int angka, int x, int y, p10_align_h_t align_h, p10_align_v_t align_v);
void p10_timestamp_to_buffer(long timestamp, int x, int y, p10_align_h_t align_h, p10_align_v_t align_v);

#endif // P10_DISPLAY_H