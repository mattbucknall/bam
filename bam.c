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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bam.h"


// ******** ASSERTIONS ********

#define BAM_MAGIC                                   0x52FFE73Eul

#ifdef BAM_DEBUG
#ifndef BAM_ASSERT

#include <assert.h>

#define BAM_ASSERT(expr)                            assert(expr)
#endif // BAM_ASSERT
#else // BAM_DEBUG
#define BAM_ASSERT(expr)                            do {} while(0)
#endif // BAM_DEBUG

#define BAM_ASSERT_CTX(_bam)                        BAM_ASSERT((_bam) && (_bam)->magic == BAM_MAGIC)

#define BAM_ASSERT_DIRTY_BUFFER_PTR(_bam, _ptr)     BAM_ASSERT((_ptr) >= (_bam)->dirty_buffer_begin && \
                                                        (_ptr) < (_bam)->dirty_buffer_end)

#define BAM_ASSERT_WIDGET_PTR(_bam, _widget)        BAM_ASSERT((_widget) >= (_bam)->widget_buffer_begin && \
                                                        (_widget) < (_bam)->widget_buffer_ptr)

#define BAM_ASSERT_WIDGET_HANDLE(_bam, _handle)     BAM_ASSERT_WIDGET_PTR((_bam), (_bam)->widget_buffer_begin + \
                                                        (_handle))


// ******** PANIC ********

static void panic(bam_t* bam, bam_panic_code_t code) {
    bam->vtable->panic(code, bam->user_data);
    BAM_ASSERT(false);
}


// ******** MIN/MAX ********

static int min_int(int a, int b) {
    return (a < b) ? a : b;
}


static int max_int(int a, int b) {
    return (a > b) ? a : b;
}


// ******** RECTANGLE ********

static void rect_init(bam_rect_t* rect, int x, int y, int width, int height) {
    BAM_ASSERT(rect);

    rect->x1 = x;
    rect->y1 = y;
    rect->x2 = x + width;
    rect->y2 = y + height;
}


static void rect_init_empty(bam_rect_t* rect) {
    BAM_ASSERT(rect);

    rect->x1 = 0;
    rect->y1 = 0;
    rect->x2 = 0;
    rect->y2 = 0;
}


static bool rect_empty(const bam_rect_t* rect) {
    BAM_ASSERT(rect);
    return rect->x2 <= rect->x1 || rect->y2 <= rect->y1;
}


static int rect_width(const bam_rect_t* rect) {
    BAM_ASSERT(rect);
    return rect->x2 - rect->x1;
}


static int rect_height(const bam_rect_t* rect) {
    BAM_ASSERT(rect);
    return rect->y2 - rect->y1;
}


static bool rect_contains_point(const bam_rect_t* rect, int x, int y) {
    BAM_ASSERT(rect);
    return (x >= rect->x1) && (x < rect->x2) && (y >= rect->y1) && (y < rect->y2);
}


static bool rect_overlaps(const bam_rect_t* rect, const bam_rect_t* other) {
    BAM_ASSERT(rect);
    BAM_ASSERT(other);

    return rect->x1 < other->x2 && other->x1 < rect->x2 &&
           rect->y1 < other->y2 && other->y1 < rect->y2;
}


static void rect_translate(bam_rect_t* rect, int dx, int dy) {
    BAM_ASSERT(rect);

    rect->x1 += dx;
    rect->y1 += dy;
    rect->x2 += dx;
    rect->y2 += dy;
}


static void rect_set_pos(bam_rect_t* rect, int x, int y) {
    BAM_ASSERT(rect);
    rect_translate(rect, x - rect->x1, y - rect->y1);
}


static void rect_intersect(bam_rect_t* rect, const bam_rect_t* other) {
    BAM_ASSERT(rect);
    BAM_ASSERT(other);

    rect->x1 = max_int(rect->x1, other->x1);
    rect->y1 = max_int(rect->y1, other->y1);
    rect->x2 = max_int(other->x1, min_int(rect->x2, other->x2));
    rect->y2 = max_int(other->y1, min_int(rect->y2, other->y2));
}


// ******** LEXICAL ANALYSIS ********

static bool lex_is_digit(char c) {
    return (c >= '0') && (c <= '9');
}


// ******** UNICODE ********

static const void* unicode_decode_utf8(const void* input, bam_unichar_t* unichar) {
    // based on public domain UTF-8 decoder here:
    // https://github.com/skeeto/branchless-utf8/blob/master/utf8.h

    static const uint8_t LENGTH_LUT[] = {
            1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1,
            0, 0, 0, 0, 0, 0, 0, 0,
            2, 2, 2, 2, 3, 3, 4, 0
    };

    static const uint8_t MASK_LUT[] = {
            0x00, 0x7f, 0x1f, 0x0f, 0x07
    };

    static const uint8_t SHIFT_C_LUT[] = {
            0, 18, 12, 6, 0
    };

    const uint8_t* seq = input;
    uint8_t len = LENGTH_LUT[seq[0] >> 3];
    const uint8_t* next = seq + len + !len;

    *unichar = (uint32_t) (seq[0] & MASK_LUT[len]) << 18;
    *unichar |= (uint32_t) (seq[1] & 0x3F) << 12;
    *unichar |= (uint32_t) (seq[2] & 0x3F) << 6;
    *unichar |= (uint32_t) (seq[3] & 0x3F);
    *unichar >>= SHIFT_C_LUT[len];

    return next;
}


// ******** FONT/GLYPH METRICS ********

static int16_t metrics_calc_string_width(bam_t* bam, const uint8_t* text_start, const uint8_t* text_end,
                                         bam_font_t font) {
    const bam_vtable_t* vtable = bam->vtable;
    const uint8_t* text_i = text_start;
    int16_t cursor_x = 0;

    while (text_i < text_end) {
        bam_unichar_t codepoint = 0;
        bam_glyph_metrics_t glyph_metrics;

        text_i = unicode_decode_utf8(text_i, &codepoint);

        if (vtable->get_glyph_metrics(&glyph_metrics, font, codepoint, bam->user_data)) {
            cursor_x = (int16_t) (cursor_x + glyph_metrics.x_advance);
        }
    }

    return cursor_x;
}


// ******** DRAWING ********

#define BAM_WIDGET_ATTR_ENABLED         ((uint32_t) 1 << 0)
#define BAM_WIDGET_ATTR_CHECKED         ((uint32_t) 1 << 1)
#define BAM_WIDGET_ATTR_PRESSED         ((uint32_t) 1 << 2)


static void draw_set_translation(bam_t* bam, int x, int y) {
    BAM_ASSERT_CTX(bam);

    bam->draw_state.translate_x = x;
    bam->draw_state.translate_y = y;
}


static void draw_set_clip(bam_t* bam, const bam_rect_t* rect) {
    BAM_ASSERT_CTX(bam);
    BAM_ASSERT(rect);

    bam_rect_t copy = *rect;

    rect_translate(&copy, bam->draw_state.translate_x, bam->draw_state.translate_y);
    rect_intersect(&bam->draw_state.clip, &copy);
}


static void draw_glyph(bam_t* bam, int x, int y, const bam_glyph_metrics_t* metrics,
                       const bam_color_pair_t* colors) {
    bam_draw_state_t* draw_state = &bam->draw_state;

    bam_rect_t dest_rect;
    bam_rect_t src_rect;

    x = (int16_t) (x + draw_state->translate_x + metrics->x_bearing);
    y = (int16_t) (y + draw_state->translate_y - metrics->y_bearing);

    rect_init(&dest_rect, x, y, metrics->width, metrics->height);
    rect_intersect(&dest_rect, &draw_state->clip);

    if (!rect_empty(&dest_rect)) {
        rect_init(&src_rect, (int16_t) (dest_rect.x1 - x), (int16_t) (dest_rect.y1 - y),
                  rect_width(&dest_rect), rect_height(&dest_rect));

        if (!rect_empty(&src_rect)) {
            bam->vtable->draw_glyph(&dest_rect, &src_rect, metrics, colors, bam->user_data);
        }
    }
}


static void draw_text(bam_t* bam, int x, int y, bam_h_align_t h_align, bam_v_align_t v_align,
                      const void* text, bam_font_t font, const bam_color_pair_t* colors) {
    const bam_vtable_t* vtable = bam->vtable;
    bam_font_metrics_t font_metrics;

    vtable->get_font_metrics(&font_metrics, font, bam->user_data);

    const uint8_t* text_i = text;
    const uint8_t* text_e = text_i + strlen(text);
    int16_t width = metrics_calc_string_width(bam, text_i, text_e, font);

    switch (h_align) {
    case BAM_H_ALIGN_CENTER:
        x = (int16_t) (x - (width / 2));
        break;

    case BAM_H_ALIGN_RIGHT:
        x = (int16_t) (x - width);

    default:
        break;
    }

    switch (v_align) {
    case BAM_V_ALIGN_TOP:
        y = (int16_t) (y + font_metrics.ascent);
        break;

    case BAM_V_ALIGN_MIDDLE:
        y = (int16_t) (y + font_metrics.center);
        break;

    case BAM_V_ALIGN_BOTTOM:
        y = (int16_t) (y - font_metrics.descent);
        break;

    default:
        break;
    }

    while (text_i < text_e) {
        bam_unichar_t codepoint = 0;
        bam_glyph_metrics_t glyph_metrics;

        text_i = unicode_decode_utf8(text_i, &codepoint);

        if (vtable->get_glyph_metrics(&glyph_metrics, font, codepoint, bam->user_data)) {
            draw_glyph(bam, x, y, &glyph_metrics, colors);
            x = (int16_t) (x + glyph_metrics.x_advance);
        }
    }
}


static void draw_fill(bam_t* bam, const bam_rect_t* rect, bam_color_t color) {
    BAM_ASSERT_CTX(bam);
    BAM_ASSERT(rect);

    bam_rect_t copy = *rect;

    rect_translate(&copy, bam->draw_state.translate_x, bam->draw_state.translate_y);
    rect_intersect(&copy, &bam->draw_state.clip);

    if (!rect_empty(&copy)) {
        bam->vtable->draw_fill(&copy, color, bam->user_data);
    }
}


static void draw_widget(bam_t* bam, const bam_widget_t* widget) {
    BAM_ASSERT_CTX(bam);
    BAM_ASSERT_WIDGET_PTR(bam, widget);
    BAM_ASSERT(((unsigned int) widget->state) < BAM_N_STATES);

    const bam_style_t* style = widget->style;
    const bam_color_pair_t* colors;
    bam_draw_state_t saved_draw_state = bam->draw_state;
    bam_rect_t inner;

    // if widget does not have valid bounding rectangle, do nothing
    if ( rect_empty(&widget->rect) ) {
        return;
    }

    // get colors for state
    colors = &style->colors[widget->state];

    // fill widget background
    draw_fill(bam, &widget->rect, colors->background);

    // calculate widget's inner region (i.e. with padding applied)
    inner = widget->rect;
    inner.x1 += style->h_padding;
    inner.y1 += style->v_padding;
    inner.x2 -= style->h_padding;
    inner.y2 -= style->v_padding;

    // only proceed with rest of drawing if there is space inside inner region
    if (!rect_empty(&inner)) {
        // clip remaining drawing to widget's inner region
        draw_set_clip(bam, &inner);

        // if widget has text, draw it
        if (widget->text[0]) {
            int text_x;
            int text_y;

            // calculate horizontal text position based on style's h_align property
            switch (style->h_align) {
            case BAM_H_ALIGN_CENTER:
                text_x = (inner.x1 + inner.x2) / 2;
                break;

            case BAM_H_ALIGN_RIGHT:
                text_x = inner.x2 - 1;
                break;

            default:
                text_x = inner.x1;
            }

            // calculate vertical text position based on style's v_align property
            switch (style->v_align) {
            case BAM_V_ALIGN_MIDDLE:
                text_y = (inner.y1 + inner.y2) / 2;
                break;

            case BAM_V_ALIGN_BOTTOM:
                text_y = inner.y2 - 1;
                break;

            default:
                text_y = inner.y1;
            }

            draw_text(bam, text_x, text_y, style->h_align, style->v_align, widget->text,
                      style->font, colors);
        }
    }

    // restore draw state
    bam->draw_state = saved_draw_state;
}


// ******** DIRTY BUFFER ********

#define BAM__UINT32_MASK            0xFFFFFFFFul

typedef void (* bam_draw_fill_t)(const bam_rect_t* dest_rect, bam_color_t color, void* user_data);

typedef void (* bam_blt_tile_t)(int x, int y, void* user_data);


static void dirty_mark_rect(bam_t* bam, const bam_rect_t* rect) {
    BAM_ASSERT_CTX(bam);
    BAM_ASSERT(rect);

    const int disp_width = bam->disp_width;
    const int disp_height = bam->disp_height;
    const int tile_width = bam->tile_width;
    const int tile_height = bam->tile_height;
    const size_t dirty_buffer_pitch = bam->dirty_buffer_pitch;

    bam_rect_t clip;
    uint32_t left_word;
    uint32_t right_word;
    uint32_t* left_ptr;
    uint32_t* right_ptr;

    clip.x1 = max_int(0, rect->x1);
    clip.y1 = max_int(0, rect->y1);
    clip.x2 = min_int(disp_width, rect->x2);
    clip.y2 = min_int(disp_height, rect->y2);

    clip.x1 /= tile_width;
    clip.y1 /= tile_height;
    clip.x2 = (clip.x2 + tile_width - 1) / tile_width;
    clip.y2 = (clip.y2 + tile_height - 1) / tile_height;

    if (!rect_empty(&clip)) {
        clip.x2--;
        clip.y2--;

        left_word = BAM__UINT32_MASK >> (clip.x1 & (BAM__UINT32_N_BITS - 1));

        left_ptr = bam->dirty_buffer_begin + ((size_t) clip.y1 * dirty_buffer_pitch) +
                   ((size_t) clip.x1 / BAM__UINT32_N_BITS);

        right_word = BAM__UINT32_MASK << (BAM__UINT32_N_BITS - 1 - (clip.x2 & (BAM__UINT32_N_BITS - 1)));

        right_ptr = bam->dirty_buffer_begin + ((size_t) clip.y1 * dirty_buffer_pitch) +
                    ((size_t) clip.x2 / BAM__UINT32_N_BITS);

        if (left_ptr == right_ptr) {
            left_word &= right_word;

            for (int y = clip.y1; y <= clip.y2; y++) {
                BAM_ASSERT_DIRTY_BUFFER_PTR(bam, left_ptr);
                *left_ptr |= left_word;
                left_ptr += dirty_buffer_pitch;
            }
        } else {
            for (int y = clip.y1; y <= clip.y2; y++) {
                BAM_ASSERT_DIRTY_BUFFER_PTR(bam, left_ptr);
                *left_ptr |= left_word;

                for (uint32_t* mid_ptr = left_ptr + 1; mid_ptr < right_ptr; mid_ptr++) {
                    BAM_ASSERT_DIRTY_BUFFER_PTR(bam, mid_ptr);
                    *mid_ptr = BAM__UINT32_MASK;
                }

                BAM_ASSERT_DIRTY_BUFFER_PTR(bam, right_ptr);
                *right_ptr |= right_word;

                left_ptr += dirty_buffer_pitch;
                right_ptr += dirty_buffer_pitch;
            }
        }
    }
}


static void dirty_mark_all(bam_t* bam) {
    BAM_ASSERT_CTX(bam);

    bam_rect_t rect;

    rect_init(&rect, 0, 0, bam->disp_width, bam->disp_height);
    dirty_mark_rect(bam, &rect);
}


static void dirty_clean(bam_t* bam) {
    BAM_ASSERT_CTX(bam);

    void* const user_data = bam->user_data;
    const bam_draw_fill_t draw_fill_func = bam->vtable->draw_fill;
    const bam_blt_tile_t blt_tile_func = bam->vtable->blt_tile;
    const int tile_width = bam->tile_width;
    const int tile_height = bam->tile_height;
    const bam_color_t background_color = bam->background_color;
    const size_t dirty_buffer_pitch = bam->dirty_buffer_pitch;
    const bam_widget_t* const widgets_begin = bam->widget_buffer_begin;
    const bam_widget_t* const widgets_end = bam->widget_buffer_ptr;
    uint32_t* dirty_i = bam->dirty_buffer_begin;
    uint32_t* dirty_m = dirty_i + dirty_buffer_pitch;
    uint32_t* dirty_e = bam->dirty_buffer_end;
    int word_x = 0;
    int offset_y = 0;
    bam_rect_t rect;

    rect_init(&rect, 0, 0, bam->tile_width, bam->tile_height);

    do {
        BAM_ASSERT_DIRTY_BUFFER_PTR(bam, dirty_i);

        uint32_t word = *dirty_i;

        *dirty_i = 0ull;

        while (word) {
            int clz = __builtin_clz(word);
            int offset_x = word_x + (clz * tile_width);
            bam_draw_state_t saved_draw_state = bam->draw_state;

            word &= ~(0x80000000ul >> clz);

            draw_fill_func(&rect, background_color, user_data);
            draw_set_translation(bam, -offset_x, -offset_y);
            rect_set_pos(&rect, offset_x, offset_y);
            draw_set_clip(bam, &rect);

            for (const bam_widget_t* widget_i = widgets_begin; widget_i < widgets_end; widget_i++) {
                if (rect_overlaps(&rect, &widget_i->rect)) {
                    draw_widget(bam, widget_i);
                }
            }

            bam->draw_state = saved_draw_state;
            rect_set_pos(&rect, 0, 0);

            blt_tile_func(offset_x, offset_y, user_data);
        }

        dirty_i++;

        if (dirty_i >= dirty_m) {
            word_x = 0;
            offset_y += tile_height;
            dirty_m += dirty_buffer_pitch;
        } else {
            word_x += 32 * tile_width;
        }
    } while (dirty_i < dirty_e);
}


// ******** WIDGET API ********

static bam_widget_t* widget_from_handle(const bam_t* bam, bam_widget_handle_t handle) {
    return bam->widget_buffer_begin + handle;
}


static const bam_style_t* widget_get_style(const bam_t* bam, const bam_style_t* style) {
    return style ? style : bam->default_style;
}


static void widget_make_dirty(bam_t* bam, const bam_widget_t* widget) {
    dirty_mark_rect(bam, &widget->rect);
}


bam_widget_handle_t bam_add_widget(bam_t* bam, int x, int y, int width, int height,
                                   const bam_style_t* style, const char* text, bool enabled) {
    BAM_ASSERT_CTX(bam);
    BAM_ASSERT(width > 0);
    BAM_ASSERT(height > 0);

    bam_widget_t* widget;

    // check if there is room in buffer for new widget
    if (bam->widget_buffer_ptr >= bam->widget_buffer_end) {
        panic(bam, BAM_PANIC_CODE_OUT_OF_MEMORY);
    }

    // allocate new widget
    widget = bam->widget_buffer_ptr;
    bam->widget_buffer_ptr++;

    // initialise widget
    widget->style = style ? style : bam->default_style;
    widget->text = text ? text : "";
    widget->state = enabled ? BAM_STATE_ENABLED : BAM_STATE_DISABLED;
    widget->callback = NULL;
    widget->user_data = NULL;

    rect_init(&widget->rect, x, y, width, height);

    // mark area of display where widget is placed as dirty
    widget_make_dirty(bam, widget);

    // return handle to new widget
    return widget - bam->widget_buffer_begin;
}


void bam_delete_widgets(bam_t* bam) {
    BAM_ASSERT_CTX(bam);

    // clear pressed widget pointer
    bam->pressed_widget = NULL;

    // reset widget buffer top
    bam->widget_buffer_ptr = bam->widget_buffer_begin;

    // assume widgets were covering most of the display, so mark whole display as dirty
    dirty_mark_all(bam);
}


void bam_force_widget_redraw(bam_t* bam, bam_widget_handle_t widget) {
    BAM_ASSERT_CTX(bam);
    BAM_ASSERT_WIDGET_HANDLE(bam, widget);

    widget_make_dirty(bam, widget_from_handle(bam, widget));
}


void bam_set_widget_callback(bam_t* bam, bam_widget_handle_t widget, bam_widget_callback_t callback,
                             void* user_data) {
    BAM_ASSERT_CTX(bam);
    BAM_ASSERT_WIDGET_HANDLE(bam, widget);

    bam_widget_t* _widget = widget_from_handle(bam, widget);

    _widget->callback = callback;
    _widget->user_data = user_data;
}


void bam_set_widget_bounds(bam_t* bam, bam_widget_handle_t widget, const bam_rect_t* bounds) {
    BAM_ASSERT_CTX(bam);
    BAM_ASSERT_WIDGET_HANDLE(bam, widget);

    bam_widget_t* _widget = widget_from_handle(bam, widget);

    widget_make_dirty(bam, _widget);
    _widget->rect = *bounds;
    widget_make_dirty(bam, _widget);
}


const bam_rect_t* bam_get_widget_bounds(bam_t* bam, bam_widget_handle_t widget) {
    BAM_ASSERT_CTX(bam);
    BAM_ASSERT_WIDGET_HANDLE(bam, widget);

    return &(widget_from_handle(bam, widget)->rect);
}


void bam_set_widget_style(bam_t* bam, bam_widget_handle_t widget, const bam_style_t* style) {
    BAM_ASSERT_CTX(bam);
    BAM_ASSERT_WIDGET_HANDLE(bam, widget);

    bam_widget_t* _widget = widget_from_handle(bam, widget);
    const bam_style_t* new_style = widget_get_style(bam, style);

    // set widget's style if different to its current style
    if (_widget->style != new_style) {
        _widget->style = new_style;
        widget_make_dirty(bam, _widget);
    }
}


const bam_style_t* bam_get_widget_style(const bam_t* bam, bam_widget_handle_t widget) {
    BAM_ASSERT_CTX(bam);
    BAM_ASSERT_WIDGET_HANDLE(bam, widget);

    return widget_from_handle(bam, widget)->style;
}


void bam_set_widget_text(bam_t* bam, bam_widget_handle_t widget, const char* text) {
    BAM_ASSERT_CTX(bam);
    BAM_ASSERT_WIDGET_HANDLE(bam, widget);

    bam_widget_t* _widget = widget_from_handle(bam, widget);

    // set widget's text if different to its current text
    text = text ? text : "";

    if (strcmp(_widget->text, text) != 0) {
        _widget->text = text;
        widget_make_dirty(bam, _widget);
    }
}


const char* bam_get_widget_text(const bam_t* bam, bam_widget_handle_t widget) {
    BAM_ASSERT_CTX(bam);
    BAM_ASSERT_WIDGET_HANDLE(bam, widget);

    return widget_from_handle(bam, widget)->text;
}


void bam_set_widget_enabled(bam_t* bam, bam_widget_handle_t widget, bool enabled) {
    BAM_ASSERT_CTX(bam);
    BAM_ASSERT_WIDGET_HANDLE(bam, widget);

    bam_widget_t* _widget = widget_from_handle(bam, widget);
    bam_state_t new_state = enabled ? BAM_STATE_ENABLED : BAM_STATE_DISABLED;

    // set widget's state if it has changed
    if (_widget->state != new_state) {
        _widget->state = new_state;
        widget_make_dirty(bam, _widget);
    }
}


bool bam_get_widget_enabled(const bam_t* bam, bam_widget_handle_t widget) {
    BAM_ASSERT_CTX(bam);
    BAM_ASSERT_WIDGET_HANDLE(bam, widget);

    return widget_from_handle(bam, widget)->state == BAM_STATE_ENABLED;
}


void bam_set_widget_metadata(bam_t* bam, bam_widget_handle_t widget, uintptr_t metadata) {
    BAM_ASSERT_CTX(bam);
    BAM_ASSERT_WIDGET_HANDLE(bam, widget);

    widget_from_handle(bam, widget)->metadata = metadata;
}


uintptr_t bam_get_widget_metadata(bam_t* bam, bam_widget_handle_t widget) {
    BAM_ASSERT_CTX(bam);
    BAM_ASSERT_WIDGET_HANDLE(bam, widget);

    return widget_from_handle(bam, widget)->metadata;
}


static bam_widget_t* widget_find_at_point(const bam_t* bam, int x, int y) {
    bam_widget_t* widget_begin = bam->widget_buffer_begin;
    bam_widget_t* widget_i = bam->widget_buffer_ptr;

    while (widget_i != widget_begin) {
        widget_i--;

        if (rect_contains_point(&widget_i->rect, x, y)) {
            return widget_i;
        }
    }

    return NULL;
}


static void widget_set_pressed(bam_t* bam, bam_widget_t* widget) {
    if (bam->pressed_widget) {
        bam->pressed_widget->state = BAM_STATE_ENABLED;
        widget_make_dirty(bam, bam->pressed_widget);
    }

    bam->pressed_widget = widget;

    if (bam->pressed_widget) {
        bam->pressed_widget->state = BAM_STATE_PRESSED;
        widget_make_dirty(bam, bam->pressed_widget);
    }
}


// ******** EVENT API ********

int bam_start(bam_t* bam) {
    BAM_ASSERT_CTX(bam);

    const bam_vtable_t* vtable = bam->vtable;
    bool* saved_run_flag;
    bool run_flag;
    bool need_clean;

    // if this is not a nested event loop, ensure quit flag is false
    if (!bam->run_flag) {
        bam->quit_flag = false;
    }

    // save pointer to previous run flag
    saved_run_flag = bam->run_flag;

    // point context at new run flag
    run_flag = true;
    bam->run_flag = &run_flag;

    // set 'need_clean' flag initially to ensure screen is up-to-date when event loop starts
    need_clean = true;

    // process events
    do {
        bam_event_t event;
        bam_widget_t* widget;
        bam_widget_t* triggered_widget;

        // reset triggered_widget pointer
        triggered_widget = NULL;

        // clean dirty buffer if an event occurred that necessitates it
        if (need_clean) {
            need_clean = false;
            dirty_clean(bam);
        }

        // clear event type so that timeouts can be detected
        event.type = BAM_EVENT_TYPE_NONE;

        // get next event
        if (vtable->get_event(&event, 100, bam->user_data)) {
            // decode event
            switch (event.type) {
            case BAM_EVENT_TYPE_QUIT:
                // stop all event loops
                bam_quit(bam, 0);
                break;

            case BAM_EVENT_TYPE_PRESS:
                // find widget at pressed coordinate
                widget = widget_find_at_point(bam, event.x, event.y);

                // if a widget was found and is enabled, set its state to pressed
                if (widget && widget->state == BAM_STATE_ENABLED) {
                    widget_set_pressed(bam, widget_find_at_point(bam, event.x, event.y));
                    need_clean = true;
                }
                break;

            case BAM_EVENT_TYPE_RELEASE:
                // if a pressed widget exists, see if it is the same widget at release coordinate
                if (bam->pressed_widget) {
                    // find widget at released coordinate
                    widget = widget_find_at_point(bam, event.x, event.y);

                    // if found widget is the pressed widget mark it as triggered
                    if (widget == bam->pressed_widget) {
                        triggered_widget = widget;
                    }
                }

                // if a pressed widget exists, return it to enabled state
                widget_set_pressed(bam, NULL);
                need_clean = true;
                break;

            default:
                break;
            }
        }

        // if a widget has been triggered and it has a callback function, dispatch it
        if (triggered_widget && triggered_widget->callback) {
            triggered_widget->callback(bam, triggered_widget - bam->widget_buffer_begin, triggered_widget->user_data);
        }
    } while (run_flag && !bam->quit_flag);

    // point context at previous run flag
    bam->run_flag = saved_run_flag;

    // return event loop result (set by bam_stop)
    return bam->run_result;
}


void bam_stop(bam_t* bam, int result) {
    BAM_ASSERT_CTX(bam);

    // only act if an event loop is currently running
    if (bam->run_flag) {
        // set run result
        bam->run_result = result;

        // clear current run flag
        *(bam->run_flag) = false;
    }
}


void bam_quit(bam_t* bam, int result) {
    // stop current event loop
    bam_stop(bam, result);

    // set quit flag - this causes all nested event loops to stop
    bam->quit_flag = true;
}


// ******** LAYOUT API ********

void bam_layout_grid(bam_t* bam, int n_cols, int n_rows, const bam_rect_t* bounds,
                     int h_spacing, int v_spacing, const bam_style_t* style, bool enabled,
                     bam_widget_handle_t handles[], size_t n_handles) {
    BAM_ASSERT_CTX(bam);
    BAM_ASSERT(bounds);
    BAM_ASSERT(handles || n_handles == 0);

    bam_widget_handle_t* handle_i = handles;
    bam_widget_handle_t* handle_e = handles + n_handles;
    bam_rect_t rect = *bounds;
    int width;
    int height;
    int x;
    int y;

    // do nothing if n_cols, n_rows ro bounds are invalid
    if (n_cols <= 0 || n_rows <= 0 || rect_empty(bounds)) {
        return;
    }

    // clip spacing
    h_spacing = max_int(0, h_spacing);
    v_spacing = max_int(0, v_spacing);

    // calculate widget width
    width = (rect_width(&rect) - (h_spacing * (n_cols - 1))) / n_cols;

    // calculate widget height
    height = (rect_height(&rect) - (v_spacing * (n_rows - 1))) / n_rows;

    // layout widgets
    y = rect.y1;

    for (int row = 0; row < n_rows; row++) {
        x = rect.x1;

        for (int col = 0; col < n_cols; col++) {
            if (handle_i >= handle_e) {
                return;
            }

            *handle_i = bam_add_widget(bam, x, y, width, height, style, NULL, enabled);
            handle_i++;

            x += width + h_spacing;
        }

        y += height + v_spacing;
    }
}


// ******** EDITOR API ********

#define BAM_EDIT_NUMBER_BUFFER_SIZE     16

typedef enum {
    BAM_NUMBER_TYPE_UNSIGNED_INT,
    BAM_NUMBER_TYPE_SIGNED_INT,
    BAM_NUMBER_TYPE_REAL
} bam_number_type_t;


typedef enum {
    BAM_EDIT_NUMBER_KEY_BACKSPACE = 3,
    BAM_EDIT_NUMBER_KEY_CLEAR = 7,
    BAM_EDIT_NUMBER_KEY_ACCEPT = 11,
    BAM_EDIT_NUMBER_KEY_DP = 12,
    BAM_EDIT_NUMBER_KEY_MINUS = 14,
    BAM_EDIT_NUMBER_KEY_CANCEL = 15
} bam_edit_number_key_t;


typedef enum {
    BAM_EDIT_NUMBER_METADATA_BACKSPACE,
    BAM_EDIT_NUMBER_METADATA_CLEAR,
    BAM_EDIT_NUMBER_METADATA_ACCEPT,
    BAM_EDIT_NUMBER_METADATA_CANCEL
} bam_edit_number_metadata_t;


typedef enum {
    BAM_EDIT_STRING_KEY_SHIFT = 30,
    BAM_EDIT_STRING_KEY_BACKSPACE = 39,
    BAM_EDIT_STRING_KEY_CANCEL = 40,
    BAM_EDIT_STRING_KEY_CLEAR = 41,
    BAM_EDIT_STRING_KEY_SPACE = 42,
    BAM_EDIT_STRING_KEY_UNUSED_BEGIN = 43,
    BAM_EDIT_STRING_KEY_UNUSED_END = 48,
    BAM_EDIT_STRING_KEY_ACCEPT = 49
} bam_edit_string_key_t;


typedef enum {
    BAM_EDIT_STRING_METADATA_CHAR,
    BAM_EDIT_STRING_METADATA_SHIFT,
    BAM_EDIT_STRING_METADATA_BACKSPACE,
    BAM_EDIT_STRING_METADATA_CANCEL,
    BAM_EDIT_STRING_METADATA_CLEAR,
    BAM_EDIT_STRING_METADATA_ACCEPT,
    BAM_EDIT_STRING_METADATA_SPACE
} bam_edit_string_metadata_t;


typedef struct {
    bam_number_type_t type;
    bam_widget_handle_t field_widget;
    bam_widget_handle_t key_widgets[16];
    char* buffer_begin;
    char* buffer_ptr;
    char* buffer_end;
} bam_edit_number_ctx_t;


typedef struct {
    bam_widget_handle_t field_widget;
    bam_widget_handle_t key_widgets[50];
    char* buffer_begin;
    char* buffer_ptr;
    char* buffer_end;
    bool allow_empty;
    bool shifted;
} bam_edit_string_ctx_t;


static void edit_number_enforce_format(bam_t* bam, bam_edit_number_ctx_t* ctx) {
    const char* str = ctx->buffer_begin;
    size_t length = ctx->buffer_ptr - ctx->buffer_begin;
    bam_widget_handle_t dp_widget = ctx->key_widgets[BAM_EDIT_NUMBER_KEY_DP];
    bam_widget_handle_t minus_widget = ctx->key_widgets[BAM_EDIT_NUMBER_KEY_MINUS];

    switch (length) {
    case 0:
        bam_set_widget_enabled(bam, dp_widget, false);
        bam_set_widget_enabled(bam, minus_widget, ctx->type != BAM_NUMBER_TYPE_UNSIGNED_INT);
        break;

    case 1:
        bam_set_widget_enabled(bam, dp_widget, ctx->type == BAM_NUMBER_TYPE_REAL &&
                                               lex_is_digit(str[0]));

        bam_set_widget_enabled(bam, minus_widget, false);
        break;

    default:
        bam_set_widget_enabled(bam, dp_widget, ctx->type == BAM_NUMBER_TYPE_REAL &&
                                               strchr(str, '.') == NULL);

        bam_set_widget_enabled(bam, minus_widget, false);
    }

    bam_set_widget_enabled(bam, ctx->key_widgets[BAM_EDIT_NUMBER_KEY_ACCEPT], length > 0 &&
                                                                              lex_is_digit(ctx->buffer_ptr[-1]));


    bam_set_widget_enabled(bam, ctx->key_widgets[BAM_EDIT_NUMBER_KEY_BACKSPACE], length > 0);
    bam_set_widget_enabled(bam, ctx->key_widgets[BAM_EDIT_NUMBER_KEY_CLEAR], length > 0);

    bam_force_widget_redraw(bam, ctx->field_widget);
}


static void edit_number_append(bam_t* bam, bam_edit_number_ctx_t* ctx, char c) {
    if (ctx->buffer_ptr < ctx->buffer_end) {
        ctx->buffer_ptr[0] = c;
        ctx->buffer_ptr++;
        ctx->buffer_ptr[0] = '\0';
        edit_number_enforce_format(bam, ctx);
    }
}


static void edit_number_key_func(bam_t* bam, bam_widget_handle_t widget, void* user_data) {
    bam_edit_number_ctx_t* ctx = user_data;
    uintptr_t metadata = bam_get_widget_metadata(bam, widget);

    switch (metadata) {
    case BAM_EDIT_NUMBER_METADATA_BACKSPACE:
        if (ctx->buffer_ptr != ctx->buffer_begin) {
            ctx->buffer_ptr--;
            ctx->buffer_ptr[0] = '\0';
            edit_number_enforce_format(bam, ctx);
        }
        break;

    case BAM_EDIT_NUMBER_METADATA_CLEAR:
        ctx->buffer_ptr = ctx->buffer_begin;
        ctx->buffer_ptr[0] = '\0';
        edit_number_enforce_format(bam, ctx);
        break;

    case BAM_EDIT_NUMBER_METADATA_ACCEPT:
        bam_stop(bam, 1);
        break;

    case BAM_EDIT_NUMBER_METADATA_CANCEL:
        bam_stop(bam, 0);
        break;

    default:
        edit_number_append(bam, ctx, (char) metadata);
    }
}


static bool edit_number(bam_t* bam, char* buffer, size_t buffer_size, bam_number_type_t type,
                        const bam_editor_style_t* editor_style) {
    static const char* KEYPAD_TEXT[16] = {
            "7", "8", "9", "",
            "4", "5", "6", "",
            "1", "2", "3", "",
            ".", "0", "-", ""
    };

    const bam_vtable_t* vtable = bam->vtable;
    const bam_style_t* style;
    int spacing = editor_style->spacing;
    bam_font_metrics_t font_metrics;
    int field_height;
    bam_rect_t bounds;
    bam_edit_number_ctx_t ctx;

    // ensure end of buffer is null-terminated
    buffer[buffer_size - 1] = '\0';

    // initialise editor context
    ctx.type = type;
    ctx.buffer_begin = buffer;
    ctx.buffer_ptr = buffer + strlen(buffer);
    ctx.buffer_end = buffer + buffer_size - 1;

    // clear any existing widgets
    bam_delete_widgets(bam);

    // get font metrics for field
    style = widget_get_style(bam, editor_style->field_style);
    vtable->get_font_metrics(&font_metrics, style->font, bam->user_data);

    // calculate field height
    field_height = font_metrics.line_height + (2 * style->v_padding);

    // if number is real, remove trailing zeros and decimal point if number is whole
    if (type == BAM_NUMBER_TYPE_REAL) {
        while (ctx.buffer_ptr > ctx.buffer_begin && ctx.buffer_ptr[-1] == '0') {
            ctx.buffer_ptr--;
        }

        if (ctx.buffer_ptr > ctx.buffer_begin && ctx.buffer_ptr[-1] == '.') {
            ctx.buffer_ptr--;
        }

        ctx.buffer_ptr[0] = '\0';
    }

    // create field widget
    ctx.field_widget = bam_add_widget(bam, 0, 0, bam->disp_width, field_height,
                                      style, buffer, false);

    // define keypad bounds
    bounds.x1 = 0;
    bounds.y1 = field_height + spacing;
    bounds.x2 = bam->disp_width;
    bounds.y2 = bam->disp_height;

    // create keypad widgets
    bam_layout_grid(bam, 4, 4, &bounds, spacing, spacing,
                    widget_get_style(bam, editor_style->num_key_style), true,
                    ctx.key_widgets, 16);

    // set numeric key text, metadata and callbacks
    for (int i = 0; i < 16; i++) {
        bam_set_widget_text(bam, ctx.key_widgets[i], KEYPAD_TEXT[i]);
        bam_set_widget_metadata(bam, ctx.key_widgets[i], KEYPAD_TEXT[i][0]);
        bam_set_widget_callback(bam, ctx.key_widgets[i], edit_number_key_func, &ctx);
    }

    // apply style/text/metadata to backspace key
    bam_set_widget_style(bam, ctx.key_widgets[BAM_EDIT_NUMBER_KEY_BACKSPACE], editor_style->edit_key_style);
    bam_set_widget_text(bam, ctx.key_widgets[BAM_EDIT_NUMBER_KEY_BACKSPACE], editor_style->backspace_text);
    bam_set_widget_metadata(bam, ctx.key_widgets[BAM_EDIT_NUMBER_KEY_BACKSPACE],
                            BAM_EDIT_NUMBER_METADATA_BACKSPACE);

    // apply style/text to clear key
    bam_set_widget_style(bam, ctx.key_widgets[BAM_EDIT_NUMBER_KEY_CLEAR], editor_style->edit_key_style);
    bam_set_widget_text(bam, ctx.key_widgets[BAM_EDIT_NUMBER_KEY_CLEAR], editor_style->clear_text);
    bam_set_widget_metadata(bam, ctx.key_widgets[BAM_EDIT_NUMBER_KEY_CLEAR],
                            BAM_EDIT_NUMBER_METADATA_CLEAR);

    // apply style/text to accept key
    bam_set_widget_style(bam, ctx.key_widgets[BAM_EDIT_NUMBER_KEY_ACCEPT], editor_style->accept_key_style);
    bam_set_widget_text(bam, ctx.key_widgets[BAM_EDIT_NUMBER_KEY_ACCEPT], editor_style->accept_text);
    bam_set_widget_metadata(bam, ctx.key_widgets[BAM_EDIT_NUMBER_KEY_ACCEPT],
                            BAM_EDIT_NUMBER_METADATA_ACCEPT);

    // apply style/text to cancel key
    bam_set_widget_style(bam, ctx.key_widgets[BAM_EDIT_NUMBER_KEY_CANCEL], editor_style->cancel_key_style);
    bam_set_widget_text(bam, ctx.key_widgets[BAM_EDIT_NUMBER_KEY_CANCEL], editor_style->cancel_text);
    bam_set_widget_metadata(bam, ctx.key_widgets[BAM_EDIT_NUMBER_KEY_CANCEL],
                            BAM_EDIT_NUMBER_METADATA_CANCEL);

    // enforce format by enabling/disabling dp/minus keys based on buffer's current content and the type of number
    // being edited
    edit_number_enforce_format(bam, &ctx);

    // start event loop
    return bam_start(bam);
}


bool bam_edit_integer(bam_t* bam, int* value, bool is_signed, const bam_editor_style_t* editor_style) {
    BAM_ASSERT_CTX(bam);
    BAM_ASSERT(value);
    BAM_ASSERT(editor_style);

    char buffer[BAM_EDIT_NUMBER_BUFFER_SIZE];
    bool accepted;

    snprintf(buffer, BAM_EDIT_NUMBER_BUFFER_SIZE, "%i", *value);

    accepted = edit_number(bam, buffer, BAM_EDIT_NUMBER_BUFFER_SIZE,
                           is_signed ? BAM_NUMBER_TYPE_SIGNED_INT : BAM_NUMBER_TYPE_UNSIGNED_INT,
                           editor_style);

    if (accepted) {
        long lvalue = strtol(buffer, NULL, 10);

        if (lvalue < INT_MIN) {
            lvalue = INT_MIN;
        } else if (lvalue > INT_MAX) {
            lvalue = INT_MAX;
        }

        *value = (int) lvalue;
    }

    return accepted;
}


bool bam_edit_real(bam_t* bam, bam_real_t* value, const bam_editor_style_t* editor_style) {
    BAM_ASSERT_CTX(bam);
    BAM_ASSERT(value);
    BAM_ASSERT(editor_style);

    char buffer[BAM_EDIT_NUMBER_BUFFER_SIZE];
    bool accepted;

    snprintf(buffer, BAM_EDIT_NUMBER_BUFFER_SIZE, "%f", *value);

    accepted = edit_number(bam, buffer, BAM_EDIT_NUMBER_BUFFER_SIZE, BAM_NUMBER_TYPE_REAL,
                           editor_style);

    if (accepted) {
        *value = strtod(buffer, NULL);
    }

    return accepted;
}


static const char* EDIT_STRING_KEYPAD_TEXT_UPPER[50] = {
        "!", "@", "#", "$", "%", "^", "&", "*", "(", ")",
        "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P",
        "A", "S", "D", "F", "G", "H", "J", "K", "L", ".",
        "", "Z", "X", "C", "V", "B", "N", "M", ",", "",
        "", "", "", "", "", "", "", "", "", ""
};


static const char* EDIT_STRING_KEYPAD_TEXT_LOWER[50] = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
        "q", "w", "e", "r", "t", "y", "u", "i", "o", "p",
        "a", "s", "d", "f", "g", "h", "j", "k", "l", ".",
        "", "z", "x", "c", "v", "b", "n", "m", ",", "",
        "", "", "", "", "", "", "", "", "", ""
};


static void edit_string_modify_char_keys(bam_t* bam, bam_edit_string_ctx_t* ctx) {
    const char** text = ctx->shifted ? EDIT_STRING_KEYPAD_TEXT_UPPER : EDIT_STRING_KEYPAD_TEXT_LOWER;

    for (int i = 0; i < 50; i++) {
        if (bam_get_widget_metadata(bam, ctx->key_widgets[i]) == BAM_EDIT_STRING_METADATA_CHAR ) {
            bam_set_widget_text(bam, ctx->key_widgets[i], text[i]);
        }
    }
}


static void edit_string_enforce_format(bam_t* bam, bam_edit_string_ctx_t* ctx) {
    const char* str = ctx->buffer_begin;
    size_t length = ctx->buffer_ptr - ctx->buffer_begin;
    size_t space = ctx->buffer_end - 1 - ctx->buffer_ptr;
    bool char_keys_changed = false;
    bool char_keys_enabled;

    if ( space == 0 && bam_get_widget_enabled(bam, ctx->key_widgets[0]) ) {
        char_keys_changed = true;
        char_keys_enabled = false;
    } else if ( space > 0 && !bam_get_widget_enabled(bam, ctx->key_widgets[0]) ) {
        char_keys_changed = true;
        char_keys_enabled = true;
    }

    if ( char_keys_changed ) {
        for (int i = 0; i < 50; i++) {
            if (bam_get_widget_metadata(bam, ctx->key_widgets[i]) == BAM_EDIT_STRING_METADATA_CHAR ) {
                bam_set_widget_enabled(bam, ctx->key_widgets[i], char_keys_enabled);
            }
        }

        bam_set_widget_enabled(bam, ctx->key_widgets[BAM_EDIT_STRING_KEY_SPACE], char_keys_enabled);
    }

    bam_set_widget_enabled(bam, ctx->key_widgets[BAM_EDIT_STRING_KEY_BACKSPACE], length > 0);
    bam_set_widget_enabled(bam, ctx->key_widgets[BAM_EDIT_STRING_KEY_CLEAR], length > 0);
    bam_set_widget_enabled(bam, ctx->key_widgets[BAM_EDIT_STRING_KEY_ACCEPT], length > 0 || ctx->allow_empty);

    bam_force_widget_redraw(bam, ctx->field_widget);
}


static void edit_string_append(bam_t* bam, bam_edit_string_ctx_t* ctx, const char* str) {
    size_t str_len = strlen(str);
    size_t space = ctx->buffer_end - 1 - ctx->buffer_ptr;

    if ( space >= str_len ) {
        memcpy(ctx->buffer_ptr, str, str_len);
        ctx->buffer_ptr += str_len;
        ctx->buffer_ptr[0] = '\0';
        edit_string_enforce_format(bam, ctx);
    }
}


static void edit_string_truncate(bam_t* bam, bam_edit_string_ctx_t* ctx) {
    while(ctx->buffer_ptr > ctx->buffer_begin) {
        ctx->buffer_ptr--;

        if ( (ctx->buffer_ptr[0] & 0xC0) != 0x80 ) {
            break;
        }
    }

    ctx->buffer_ptr[0] = '\0';
    edit_string_enforce_format(bam, ctx);
}


static void edit_string_clear(bam_t* bam, bam_edit_string_ctx_t* ctx) {
    ctx->buffer_ptr = ctx->buffer_begin;
    ctx->buffer_ptr[0] = '\0';
    edit_string_enforce_format(bam, ctx);
}


static void edit_string_key_func(bam_t* bam, bam_widget_handle_t widget, void* user_data) {
    bam_edit_string_ctx_t* ctx = user_data;
    uintptr_t metadata = bam_get_widget_metadata(bam, widget);

    switch(metadata) {
    case BAM_EDIT_STRING_METADATA_CHAR:
        edit_string_append(bam, ctx, bam_get_widget_text(bam, widget));
        break;

    case BAM_EDIT_STRING_METADATA_SHIFT:
        ctx->shifted = !(ctx->shifted);
        edit_string_modify_char_keys(bam, ctx);
        break;

    case BAM_EDIT_STRING_METADATA_BACKSPACE:
        edit_string_truncate(bam, ctx);
        break;

    case BAM_EDIT_STRING_METADATA_CANCEL:
        bam_stop(bam, 0);
        break;

    case BAM_EDIT_STRING_METADATA_CLEAR:
        edit_string_clear(bam, ctx);
        break;

    case BAM_EDIT_STRING_METADATA_ACCEPT:
        bam_stop(bam, 1);
        break;

    case BAM_EDIT_STRING_METADATA_SPACE:
        edit_string_append(bam, ctx, " ");
        break;

    default:
        break;
    }
}


bool bam_edit_string(bam_t* bam, char* buffer, size_t buffer_size, bool allow_empty,
                     const bam_editor_style_t* editor_style) {
    BAM_ASSERT_CTX(bam);
    BAM_ASSERT(buffer);
    BAM_ASSERT(buffer_size > 1);
    BAM_ASSERT(editor_style);

    const bam_vtable_t* vtable = bam->vtable;
    const bam_style_t* style;
    int spacing = editor_style->spacing;
    bam_font_metrics_t font_metrics;
    int field_height;
    bam_rect_t bounds;
    const bam_rect_t* bounds_ptr;
    bam_edit_string_ctx_t ctx;

    // ensure end of buffer is null-terminated
    buffer[buffer_size - 1] = '\0';

    // initialise editor context
    ctx.buffer_begin = buffer;
    ctx.buffer_ptr = buffer + strlen(buffer);
    ctx.buffer_end = buffer + buffer_size;
    ctx.allow_empty = allow_empty;
    ctx.shifted = false;

    // clear any existing widgets
    bam_delete_widgets(bam);

    // get font metrics for field
    style = widget_get_style(bam, editor_style->field_style);
    vtable->get_font_metrics(&font_metrics, style->font, bam->user_data);

    // calculate field height
    field_height = font_metrics.line_height + (2 * style->v_padding);

    // create field widget
    ctx.field_widget = bam_add_widget(bam, 0, 0, bam->disp_width, field_height,
                                      style, buffer, false);

    // define keypad bounds
    bounds.x1 = 0;
    bounds.y1 = field_height + spacing;
    bounds.x2 = bam->disp_width;
    bounds.y2 = bam->disp_height;

    // create keypad widgets
    bam_layout_grid(bam, 10, 5, &bounds, spacing, spacing,
                    widget_get_style(bam, editor_style->char_key_style), true,
                    ctx.key_widgets, 50);

    // set letter/symbol key text, metadata and callbacks
    for (int i = 0; i < 50; i++) {
        bam_set_widget_metadata(bam, ctx.key_widgets[i], BAM_EDIT_STRING_METADATA_CHAR);
        bam_set_widget_callback(bam, ctx.key_widgets[i], edit_string_key_func, &ctx);
    }

    // apply num key style to top row of keys
    for (int i = 0; i < 10; i++) {
        bam_set_widget_style(bam, ctx.key_widgets[i], editor_style->num_key_style);
    }

    edit_string_modify_char_keys(bam, &ctx);
    
    // apply style/text/metadata to shift key
    bam_set_widget_style(bam, ctx.key_widgets[BAM_EDIT_STRING_KEY_SHIFT], editor_style->edit_key_style);
    bam_set_widget_text(bam, ctx.key_widgets[BAM_EDIT_STRING_KEY_SHIFT], editor_style->shift_text);
    bam_set_widget_metadata(bam, ctx.key_widgets[BAM_EDIT_STRING_KEY_SHIFT], 
                            BAM_EDIT_STRING_METADATA_SHIFT);
    
    // apply style/text/metadata to backspace key
    bam_set_widget_style(bam, ctx.key_widgets[BAM_EDIT_STRING_KEY_BACKSPACE], editor_style->edit_key_style);
    bam_set_widget_text(bam, ctx.key_widgets[BAM_EDIT_STRING_KEY_BACKSPACE], editor_style->backspace_text);
    bam_set_widget_metadata(bam, ctx.key_widgets[BAM_EDIT_STRING_KEY_BACKSPACE],
                            BAM_EDIT_STRING_METADATA_BACKSPACE);

    // apply style/text/metadata to cancel key
    bam_set_widget_style(bam, ctx.key_widgets[BAM_EDIT_STRING_KEY_CANCEL], editor_style->cancel_key_style);
    bam_set_widget_text(bam, ctx.key_widgets[BAM_EDIT_STRING_KEY_CANCEL], editor_style->cancel_text);
    bam_set_widget_metadata(bam, ctx.key_widgets[BAM_EDIT_STRING_KEY_CANCEL],
                            BAM_EDIT_STRING_METADATA_CANCEL);

    // apply style/text/metadata to clear key
    bam_set_widget_style(bam, ctx.key_widgets[BAM_EDIT_STRING_KEY_CLEAR], editor_style->edit_key_style);
    bam_set_widget_text(bam, ctx.key_widgets[BAM_EDIT_STRING_KEY_CLEAR], editor_style->clear_text);
    bam_set_widget_metadata(bam, ctx.key_widgets[BAM_EDIT_STRING_KEY_CLEAR],
                            BAM_EDIT_STRING_METADATA_CLEAR);

    // apply style/text/metadata to accept key
    bam_set_widget_style(bam, ctx.key_widgets[BAM_EDIT_STRING_KEY_ACCEPT], editor_style->accept_key_style);
    bam_set_widget_text(bam, ctx.key_widgets[BAM_EDIT_STRING_KEY_ACCEPT], editor_style->accept_text);
    bam_set_widget_metadata(bam, ctx.key_widgets[BAM_EDIT_STRING_KEY_ACCEPT],
                            BAM_EDIT_STRING_METADATA_ACCEPT);

    // apply style/text/metadata to space key
    bam_set_widget_style(bam, ctx.key_widgets[BAM_EDIT_STRING_KEY_SPACE], editor_style->char_key_style);
    bam_set_widget_text(bam, ctx.key_widgets[BAM_EDIT_STRING_KEY_SPACE], editor_style->space_text);
    bam_set_widget_metadata(bam, ctx.key_widgets[BAM_EDIT_STRING_KEY_SPACE],
                            BAM_EDIT_STRING_METADATA_SPACE);

    // stretch space key to span across unused widgets
    bounds_ptr = bam_get_widget_bounds(bam, ctx.key_widgets[BAM_EDIT_STRING_KEY_SPACE]);
    bounds.x1 = bounds_ptr->x1;
    bounds.y1 = bounds_ptr->y1;
    bounds_ptr = bam_get_widget_bounds(bam, ctx.key_widgets[BAM_EDIT_STRING_KEY_UNUSED_END]);
    bounds.x2 = bounds_ptr->x2;
    bounds.y2 = bounds_ptr->y2;

    bam_set_widget_bounds(bam, ctx.key_widgets[BAM_EDIT_STRING_KEY_SPACE], &bounds);

    // invalidate unused widgets
    rect_init_empty(&bounds);

    for (int i = BAM_EDIT_STRING_KEY_UNUSED_BEGIN; i <= BAM_EDIT_STRING_KEY_UNUSED_END; i++) {
        bam_set_widget_bounds(bam, ctx.key_widgets[i], &bounds);
    }

    return bam_start(bam);
}


// ******** CONTEXT API ********

void bam_init(bam_t* bam, uint32_t* dirty_buffer, size_t dirty_buffer_size,
              bam_widget_t* widget_buffer, size_t widget_buffer_size,
              int disp_width, int disp_height, int tile_width, int tile_height,
              bam_color_t background_color, const bam_style_t* default_style,
              const bam_vtable_t* vtable, void* user_data) {
    BAM_ASSERT(bam);
    BAM_ASSERT(dirty_buffer);
    BAM_ASSERT(dirty_buffer_size > 0);
    BAM_ASSERT(widget_buffer);
    BAM_ASSERT(widget_buffer > 0);
    BAM_ASSERT(disp_width > 0);
    BAM_ASSERT(disp_height > 0);
    BAM_ASSERT(tile_width > 0);
    BAM_ASSERT(tile_height > 0);
    BAM_ASSERT(default_style);
    BAM_ASSERT(vtable);
    BAM_ASSERT(vtable->panic);
    BAM_ASSERT(vtable->get_monotonic_time);
    BAM_ASSERT(vtable->get_event);
    BAM_ASSERT(vtable->get_font_metrics);
    BAM_ASSERT(vtable->get_glyph_metrics);
    BAM_ASSERT(vtable->draw_glyph);
    BAM_ASSERT(vtable->draw_fill);
    BAM_ASSERT(vtable->blt_tile);

    // ensure context structure is clean
    memset(bam, 0, sizeof(*bam));

    // initialise context structure
    bam->dirty_buffer_begin = dirty_buffer;
    bam->dirty_buffer_end = dirty_buffer + dirty_buffer_size;
    bam->dirty_buffer_pitch = BAM__TILE_PITCH(BAM__TILE_COUNT(disp_width, tile_width));

    bam->widget_buffer_begin = widget_buffer;
    bam->widget_buffer_end = widget_buffer + widget_buffer_size;
    bam->widget_buffer_ptr = widget_buffer;

    bam->disp_width = disp_width;
    bam->disp_height = disp_height;
    bam->tile_width = tile_width;
    bam->tile_height = tile_height;

    bam->background_color = background_color;
    bam->default_style = default_style;

    bam->draw_state.translate_x = 0;
    bam->draw_state.translate_y = 0;
    bam->draw_state.clip.x1 = 0;
    bam->draw_state.clip.y1 = 0;
    bam->draw_state.clip.x2 = disp_width;
    bam->draw_state.clip.y2 = disp_height;

    bam->vtable = vtable;
    bam->user_data = user_data;

    bam->quit_flag = false;
    bam->run_flag = NULL;
    bam->run_result = 0;

    bam->pressed_widget = NULL;

    // check dirty buffer size
    if (dirty_buffer_size < BAM_DIRTY_BUFFER_SIZE(disp_width, disp_height, tile_width, tile_height)) {
        panic(bam, BAM_PANIC_CODE_DIRTY_BUFFER_TOO_SMALL);
    }

    // optionally mark context structure as ready
#ifdef BAM_DEBUG
    bam->magic = BAM_MAGIC;
#endif // BAM_DEBUG

    // mark whole display as dirty
    dirty_mark_all(bam);
}
