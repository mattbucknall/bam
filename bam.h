/*
 *  ___     _     __  __   _
 * | _ )   /_\   |  \/  | | |
 * | _ \  / _ \  | |\/| | |_|
 * |___/ /_/ \_\ |_|  |_| (_)
 *
 * A lightweight hardware agnostic touchscreen GUI library for embedded systems.
 *
 * https://github.com/mattbucknall/bam
 *
 * Copyright (C) 2022 Matthew T. Bucknall
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _BAM_H_
#define _BAM_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


// ******** CONTEXT TYPE ********

typedef struct bam bam_t;


// ******** PLATFORM TYPES ********

typedef double bam_real_t;


// ******** ERROR TYPES ********

typedef enum {
    BAM_PANIC_CODE_UNDEFINED,
    BAM_PANIC_CODE_DIRTY_BUFFER_TOO_SMALL,
    BAM_PANIC_CODE_OUT_OF_MEMORY,
    BAM_PANIC_CODE_INVALID_WIDGET_HANDLE
} bam_panic_code_t;


// ******** FONT/TEXT TYPES ********

typedef uint32_t bam_unichar_t;


typedef const void* bam_font_t;


typedef struct {
    int ascent;
    int descent;
    int center;
    int line_height;
} bam_font_metrics_t;


typedef struct {
    bam_unichar_t codepoint;
    int width;
    int height;
    int x_bearing;
    int y_bearing;
    int x_advance;
    void* user_data;
} bam_glyph_metrics_t;


// ******** LAYOUT/STYLE TYPES ********

#define BAM_N_STATES            3


typedef enum {
    BAM_STATE_DISABLED,
    BAM_STATE_ENABLED,
    BAM_STATE_PRESSED
} bam_state_t;


typedef enum {
    BAM_H_ALIGN_LEFT,
    BAM_H_ALIGN_CENTER,
    BAM_H_ALIGN_RIGHT
} bam_h_align_t;


typedef enum {
    BAM_V_ALIGN_TOP,
    BAM_V_ALIGN_MIDDLE,
    BAM_V_ALIGN_BOTTOM
} bam_v_align_t;


typedef uint32_t bam_color_t;


typedef struct {
    bam_color_t foreground;
    bam_color_t background;
} bam_color_pair_t;


typedef struct {
    bam_font_t font;
    bam_h_align_t h_align;
    bam_v_align_t v_align;
    int h_padding;
    int v_padding;
    bam_color_pair_t colors[BAM_N_STATES];
} bam_style_t;


typedef struct {
    int x1;
    int y1;
    int x2;
    int y2;
} bam_rect_t;


// ******** EVENT TYPES ********

typedef enum {
    BAM_EVENT_TYPE_NONE,
    BAM_EVENT_TYPE_QUIT,
    BAM_EVENT_TYPE_PRESS,
    BAM_EVENT_TYPE_RELEASE
} bam_event_type_t;


typedef struct {
    bam_event_type_t type;
    int x;
    int y;
} bam_event_t;


typedef uint16_t bam_tick_t;


// ******** WIDGET TYPES ********

typedef size_t bam_widget_handle_t;

typedef void (*bam_widget_callback_t) (bam_t* bam, bam_widget_handle_t widget, void* user_data);

typedef struct bam_widget bam_widget_t;


// ******** VTABLE ********

typedef struct {
    void (* panic) (bam_panic_code_t code, void* user_data);

    bam_tick_t (* get_monotonic_time) (void* user_data);

    bool (* get_event) (bam_event_t* event, bam_tick_t timeout, void* user_data);

    void (* get_font_metrics) (bam_font_metrics_t* metrics, bam_font_t font, void* user_data);

    bool (* get_glyph_metrics) (bam_glyph_metrics_t* metrics, bam_font_t font, bam_unichar_t codepoint,
            void* user_data);

    void (* draw_glyph) (const bam_rect_t* dest_rect, const bam_rect_t* src_rect, const bam_glyph_metrics_t* metrics,
            const bam_color_pair_t* colors, void* user_data);

    void (* draw_fill) (const bam_rect_t* dest_rect, bam_color_t color, void* user_data);

    void (* blt_tile) (int x, int y, void* user_data);
} bam_vtable_t;


// ******** CONTEXT API ********

#define BAM_DIRTY_BUFFER_SIZE(disp_width, disp_height, tile_width, tile_height) \
        BAM__DIRTY_BUFFER_SIZE((disp_width), (disp_height), (tile_width), (tile_height))


void bam_init(bam_t* bam, uint32_t* dirty_buffer, size_t dirty_buffer_size,
              bam_widget_t* widget_buffer, size_t widget_buffer_size,
              int disp_width, int disp_height, int tile_width, int tile_height,
              bam_color_t background_color, const bam_style_t* default_style,
              const bam_vtable_t* vtable, void* user_data);


// ******** WIDGET API ********

bam_widget_handle_t bam_add_widget(bam_t* bam, int x, int y, int width, int height,
                                   const bam_style_t* style, const char* text, bool enabled);

void bam_delete_widgets(bam_t* bam);

void bam_force_widget_redraw(bam_t* bam, bam_widget_handle_t widget);

void bam_set_widget_callback(bam_t* bam, bam_widget_handle_t widget, bam_widget_callback_t callback,
                             void* user_data);

void bam_set_widget_style(bam_t* bam, bam_widget_handle_t widget, const bam_style_t* style);

const bam_style_t* bam_get_widget_style(const bam_t* bam, bam_widget_handle_t widget);

void bam_set_widget_text(bam_t* bam, bam_widget_handle_t widget, const char* text);

const char* bam_get_widget_text(const bam_t* bam, bam_widget_handle_t widget);

void bam_set_widget_enabled(bam_t* bam, bam_widget_handle_t widget, bool enabled);

bool bam_get_widget_enabled(const bam_t* bam, bam_widget_handle_t widget);

void bam_set_widget_metadata(bam_t* bam, bam_widget_handle_t widget, uintptr_t metadata);

uintptr_t bam_get_widget_metadata(bam_t* bam, bam_widget_handle_t widget);


// ******** EVENT API ********

int bam_start(bam_t* bam);

void bam_stop(bam_t* bam, int result);

void bam_quit(bam_t* bam, int result);


// ******** LAYOUT API ********

void bam_layout_grid(bam_t* bam, int n_cols, int n_rows, const bam_rect_t* bounds,
                     int h_spacing, int v_spacing, const bam_style_t* style, bool enabled,
                     bam_widget_handle_t handles[], size_t n_handles);


// ******** EDITOR API ********

typedef struct {
    const bam_style_t* char_key_style;
    const bam_style_t* edit_key_style;
    const bam_style_t* accept_key_style;
    const bam_style_t* cancel_key_style;
    const bam_style_t* field_style;
    const char* shift_text;
    const char* backspace_text;
    const char* clear_text;
    const char* accept_text;
    const char* cancel_text;
    int spacing;
} bam_editor_style_t;


bool bam_edit_integer(bam_t* bam, int* value, bool is_signed, const bam_editor_style_t* editor_style);

bool bam_edit_real(bam_t* bam, bam_real_t* value, const bam_editor_style_t* editor_style);


// ******** IMPLEMENTATION (PRIVATE) ********

#define BAM__UINT32_N_BITS              (sizeof(uint32_t) * 8)

#define BAM__TILE_COUNT(disp, tile)     (((int)(disp) + (int)(tile) - 1) / (int)(tile))

#define BAM__TILE_PITCH(n_tile_cols)    (((int)(n_tile_cols) + BAM__UINT32_N_BITS - 1) / BAM__UINT32_N_BITS)

#define BAM__DIRTY_BUFFER_SIZE(disp_width, disp_height, tile_width, tile_height) \
        (BAM__TILE_PITCH(BAM__TILE_COUNT((disp_width), (tile_width))) * BAM__TILE_COUNT((disp_height), (tile_height)))


struct bam_widget {
    const bam_style_t* style;
    const char* text;
    bam_state_t state;
    bam_rect_t rect;
    bam_widget_callback_t callback;
    void* user_data;
    uintptr_t metadata;
};


typedef struct {
    int translate_x;
    int translate_y;
    bam_rect_t clip;
} bam_draw_state_t;


struct bam {
#ifdef BAM_DEBUG
    uint32_t magic;
#endif // BAM_DEBUG

    uint32_t* dirty_buffer_begin;
    uint32_t* dirty_buffer_end;
    size_t dirty_buffer_pitch;

    bam_widget_t* widget_buffer_begin;
    bam_widget_t* widget_buffer_end;
    bam_widget_t* widget_buffer_ptr;

    int disp_width;
    int disp_height;
    int tile_width;
    int tile_height;

    bam_color_t background_color;
    const bam_style_t* default_style;

    const bam_vtable_t* vtable;
    void* user_data;

    bam_draw_state_t draw_state;

    bool quit_flag;
    bool* run_flag;
    int run_result;

    bam_widget_t* pressed_widget;
};


#endif // _BAM_H_
