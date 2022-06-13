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

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <bam.h>
#include <font2c-types.h>
#include <SDL.h>


// ******** DEMO CONSTANTS ********

#define APP_DISPLAY_WIDTH           800
#define APP_DISPLAY_HEIGHT          480
#define APP_TILE_WIDTH              32
#define APP_TILE_HEIGHT             32

#define APP_DIRTY_BUFFER_SIZE       BAM_DIRTY_BUFFER_SIZE(APP_DISPLAY_WIDTH, APP_DISPLAY_HEIGHT, \
                                        APP_TILE_WIDTH, APP_TILE_HEIGHT)

#define APP_WIDGET_BUFFER_SIZE      64


// ******** STYLE DATA ********

// defined in font-deja-vu-sans-64.c
extern const font2c_font_t font_deja_vu_sans_64;


#define APP_COLOR_BLACK             0xFF000000ul
#define APP_COLOR_WHITE             0xFFFFFFFFul
#define APP_COLOR_DARK_GRAY         0xFF202020ul
#define APP_COLOR_GRAY              0xFF303030ul
#define APP_COLOR_MED_GRAY          0xFF606060ul
#define APP_COLOR_LIGHT_GRAY        0xFFC0C0C0ul
#define APP_COLOR_DARK_BLUE         0xFF500000ul
#define APP_COLOR_BLUE              0xFFA00000ul
#define APP_COLOR_LIGHT_BLUE        0xFFD00000ul
#define APP_COLOR_DARK_GREEN        0xFF005000ul
#define APP_COLOR_GREEN             0xFF00A000ul
#define APP_COLOR_LIGHT_GREEN       0xFF00D000ul
#define APP_COLOR_DARK_RED          0xFF000050ul
#define APP_COLOR_RED               0xFF0000A0ul
#define APP_COLOR_LIGHT_RED         0xFF0000D0ul


static const bam_style_t APP_DEFAULT_STYLE = { // NOLINT(cppcoreguidelines-interfaces-global-init)
        .font = &font_deja_vu_sans_64,
        .h_align = BAM_H_ALIGN_CENTER,
        .v_align = BAM_V_ALIGN_MIDDLE,
        .h_padding = 4,
        .v_padding = 4,
        .colors = {
                {
                        // disabled
                        .foreground = APP_COLOR_DARK_GRAY,
                        .background = APP_COLOR_GRAY
                },
                {
                        // enabled
                        .foreground = APP_COLOR_WHITE,
                        .background = APP_COLOR_MED_GRAY
                },
                {
                        // pressed
                        .foreground = APP_COLOR_WHITE,
                        .background = APP_COLOR_LIGHT_BLUE
                }
        }
};


static const bam_style_t APP_NUM_FIELD_STYLE = { // NOLINT(cppcoreguidelines-interfaces-global-init)
        .font = &font_deja_vu_sans_64,
        .h_align = BAM_H_ALIGN_RIGHT,
        .v_align = BAM_V_ALIGN_MIDDLE,
        .h_padding = 4,
        .v_padding = 4,
        .colors = {
                {
                        // disabled
                        .foreground = APP_COLOR_WHITE,
                        .background = APP_COLOR_DARK_GRAY
                },
                {
                        // enabled
                        .foreground = APP_COLOR_WHITE,
                        .background = APP_COLOR_DARK_GRAY
                }
        }
};


static const bam_style_t APP_EDIT_STYLE = { // NOLINT(cppcoreguidelines-interfaces-global-init)
        .font = &font_deja_vu_sans_64,
        .h_align = BAM_H_ALIGN_CENTER,
        .v_align = BAM_V_ALIGN_MIDDLE,
        .colors = {
                {
                        // disabled
                        .foreground = APP_COLOR_BLUE,
                        .background = APP_COLOR_DARK_BLUE,
                },
                {
                        // enabled
                        .foreground = APP_COLOR_WHITE,
                        .background = APP_COLOR_BLUE,
                },
                {
                        // pressed
                        .foreground = APP_COLOR_WHITE,
                        .background = APP_COLOR_LIGHT_BLUE
                }
        }
};


static const bam_style_t APP_ACCEPT_STYLE = { // NOLINT(cppcoreguidelines-interfaces-global-init)
        .font = &font_deja_vu_sans_64,
        .h_align = BAM_H_ALIGN_CENTER,
        .v_align = BAM_V_ALIGN_MIDDLE,
        .colors = {
                {
                        // disabled
                        .foreground = APP_COLOR_GREEN,
                        .background = APP_COLOR_DARK_GREEN,
                },
                {
                        // enabled
                        .foreground = APP_COLOR_WHITE,
                        .background = APP_COLOR_GREEN,
                },
                {
                        // pressed
                        .foreground = APP_COLOR_WHITE,
                        .background = APP_COLOR_LIGHT_GREEN
                }
        }
};


static const bam_style_t APP_CANCEL_STYLE = { // NOLINT(cppcoreguidelines-interfaces-global-init)
        .font = &font_deja_vu_sans_64,
        .h_align = BAM_H_ALIGN_CENTER,
        .v_align = BAM_V_ALIGN_MIDDLE,
        .colors = {
                {
                        // disabled
                        .foreground = APP_COLOR_RED,
                        .background = APP_COLOR_DARK_RED,
                },
                {
                        // enabled
                        .foreground = APP_COLOR_WHITE,
                        .background = APP_COLOR_RED,
                },
                {
                        // pressed
                        .foreground = APP_COLOR_WHITE,
                        .background = APP_COLOR_LIGHT_RED
                }
        }
};


static const bam_editor_style_t APP_EDITOR_STYLE = {
        .char_key_style = &APP_DEFAULT_STYLE,
        .edit_key_style = &APP_EDIT_STYLE,
        .accept_key_style = &APP_ACCEPT_STYLE,
        .cancel_key_style = &APP_CANCEL_STYLE,
        .field_style = &APP_NUM_FIELD_STYLE,
        .shift_text = "SH",
        .backspace_text = "BS",
        .clear_text = "CL",
        .accept_text = "OK",
        .cancel_text = "CAN",
        .spacing = 4
};


// ******** GLOBAL VARIABLES ********

static SDL_Window* m_window;
static SDL_Surface* m_surface;
static SDL_Surface* m_tile;
static jmp_buf m_panic_jmp;
static bam_t m_bam;
static bool m_update_surface;


// ******** VTABLE FUNCTION IMPLEMENTATIONS ********

static void v_panic(bam_panic_code_t code, void* user_data) {
    // unused arguments
    (void) user_data;

    // print error message
    fprintf(stderr, "BaM Panic: %i\n", (int) code);

    // long jump to error handler in main function (this function is not allowed to return to its caller)
    longjmp(m_panic_jmp, 1);
}


static bam_tick_t v_get_monotonic_time(void* user_data) {
    // unused arguments
    (void) user_data;

    // get monotonic time using SDL library
    return (bam_tick_t) SDL_GetTicks();
}


static bool v_get_event(bam_event_t* event, bam_tick_t timeout, void* user_data) {
    uint32_t now;
    uint32_t start_time;
    uint32_t elapsed_ms;
    SDL_Event s_event;

    // timestamp when function was called (point of time to which timeout is relative)
    now = SDL_GetTicks();
    start_time = now;

    // loop until a timeout or event relevant to BaM occurs
    for(;;) {
        if ( m_update_surface ) {
            m_update_surface = false;
            SDL_UpdateWindowSurface(m_window);
        }

        // calculate time that has elapsed since function was called
        elapsed_ms = (uint32_t) (now - start_time);

        // timeout if elapsed time is greater than or equal to specified timeout period
        if ( elapsed_ms >= timeout ) {
            return false;
        }

        // wait for input event up until timoue is due
        s_event.type = SDL_FIRSTEVENT;
        SDL_WaitEventTimeout(&s_event, (int) (timeout - elapsed_ms));

        // translate SDL event into BaM event or ignore if not relevant
        switch(s_event.type) {
        case SDL_FIRSTEVENT:
            // timeout occurred
            return false;

        case SDL_QUIT:
            event->type = BAM_EVENT_TYPE_QUIT;
            return true;

        case SDL_MOUSEBUTTONDOWN:
            if ( s_event.button.button == SDL_BUTTON_LEFT ) {
                event->type = BAM_EVENT_TYPE_PRESS;
                event->x = s_event.button.x;
                event->y = s_event.button.y;
                return true;
            }
            break;

        case SDL_MOUSEBUTTONUP:
            if ( s_event.button.button == SDL_BUTTON_LEFT ) {
                event->type = BAM_EVENT_TYPE_RELEASE;
                event->x = s_event.button.x;
                event->y = s_event.button.y;
                return true;
            }
            break;

        default:
            break;
        }

        // update 'now' timestamp for calculating time left until timeout in next iteration
        now = SDL_GetTicks();
    }
}


static void v_get_font_metrics(bam_font_metrics_t* metrics, bam_font_t font, void* user_data) {
    // unused arguments
    (void) user_data;

    // translate font2c font metrics into BaM font metrics
    const font2c_font_t* f2c_font = (const font2c_font_t*) font;

    metrics->ascent = f2c_font->ascent;
    metrics->descent = f2c_font->descent;
    metrics->center = f2c_font->center;
    metrics->line_height = f2c_font->line_height;
}


static bool v_get_glyph_metrics(bam_glyph_metrics_t* metrics, bam_font_t font, bam_unichar_t codepoint,
                                void* user_data) {
    const font2c_font_t* f2c_font;
    const font2c_glyph_t* f2c_glyph;

    // unused arguments
    (void) user_data;

    // treat font as pointer to font2c font structure
    f2c_font = (const font2c_font_t*) font;

    // find f2c_glyph for given codepoint
    f2c_glyph = font2c_find_glyph(f2c_font, codepoint);

    // return false if f2c_glyph could not be found
    if ( !f2c_glyph ) {
        return false;
    }

    // translate font2c f2c_glyph metrics into Bam f2c_glyph metrics
    metrics->codepoint = codepoint;
    metrics->width = f2c_glyph->width;
    metrics->height = f2c_glyph->height;
    metrics->x_bearing = f2c_glyph->x_bearing;
    metrics->y_bearing = f2c_glyph->y_bearing;
    metrics->x_advance = f2c_glyph->x_advance;

    // point metrics user_data at start of font2c f2c_glyph's bitmap data
    metrics->user_data = (void*) (f2c_font->pixels + f2c_glyph->offset);

    return true;
}


static uint8_t interpolate_u8(uint8_t start, uint8_t finish, uint8_t k) {
    return start + ((k * (finish - start) + 1) >> 4);
}


static void gen_color_lut(bam_color_t* lut, bam_color_t fg_color, bam_color_t bg_color) {
    // NOTE: assumes little-endian architecture
    typedef union {
        uint32_t word;
        uint8_t channels[4];
    } decomposed_t;

    decomposed_t fg;
    decomposed_t bg;
    decomposed_t inter;

    fg.word = fg_color;
    bg.word = bg_color;
    inter.channels[3] = 0xFF;

    // calculate 16 step linear gradient between foreground color and background color
    for (unsigned int k = 0; k < 16; k++) {
        inter.channels[0] = interpolate_u8(bg.channels[0], fg.channels[0], k);
        inter.channels[1] = interpolate_u8(bg.channels[1], fg.channels[1], k);
        inter.channels[2] = interpolate_u8(bg.channels[2], fg.channels[2], k);

        lut[k] = inter.word;
    }
}


static void v_draw_glyph(const bam_rect_t* dest_rect, const bam_rect_t* src_rect, const bam_glyph_metrics_t* metrics,
                         const bam_color_pair_t* colors, void* user_data) {
    static bam_color_t prev_foreground;
    static bam_color_t prev_background;
    static bam_color_t lut[16];

    bam_color_t foreground = colors->foreground;
    bam_color_t background = colors->background;

    (void) user_data;

    // regenerate color interpolation LUT if requests colors have changed since last call
    if (foreground != prev_foreground || background != prev_background) {
        gen_color_lut(lut, foreground, background);
        prev_foreground = foreground;
        prev_background = background;
    }

    // precalculate blt parameters
    size_t src_pitch = (metrics->width + 1) / 2;
    const uint8_t* src_rows_i = ((const uint8_t*) metrics->user_data) + (src_rect->x1 / 2) + (src_rect->y1 * src_pitch);

    size_t dest_pitch = (m_tile->pitch) / sizeof(uint32_t);
    size_t dest_width = dest_rect->x2 - dest_rect->x1;
    uint32_t* dest_rows_i = ((uint32_t*) m_tile->pixels) + dest_rect->x1 + (dest_pitch * dest_rect->y1);
    uint32_t* dest_rows_e = ((uint32_t*) m_tile->pixels) + dest_rect->x1 + (dest_pitch * dest_rect->y2);

    // source image is packed as two horizontally adjacent pixels per byte, so read/increment order depends on whether
    // src_rect.x1 is odd or even
    if ( (src_rect->x1 & 1) == 0 ) {
        while (dest_rows_i < dest_rows_e) {
            const uint8_t* src_pixels_i = src_rows_i;
            uint32_t* dest_pixels_i = dest_rows_i;
            uint32_t* dest_pixels_e = dest_rows_i + dest_width;

            for (;;) {
                if (dest_pixels_i >= dest_pixels_e) {
                    break;
                }

                *dest_pixels_i++ = lut[*src_pixels_i & 0x0F];

                if (dest_pixels_i >= dest_pixels_e) {
                    break;
                }

                *dest_pixels_i++ = lut[*src_pixels_i++ >> 4];
            }

            src_rows_i += src_pitch;
            dest_rows_i += dest_pitch;
        }
    } else {
        while (dest_rows_i < dest_rows_e) {
            const uint8_t* src_pixels_i = src_rows_i;
            uint32_t* dest_pixels_i = dest_rows_i;
            uint32_t* dest_pixels_e = dest_rows_i + dest_width;

            for (;;) {
                if (dest_pixels_i >= dest_pixels_e) {
                    break;
                }

                *dest_pixels_i++ = lut[*src_pixels_i++ >> 4];

                if (dest_pixels_i >= dest_pixels_e) {
                    break;
                }

                *dest_pixels_i++ = lut[*src_pixels_i & 0x0F];
            }

            src_rows_i += src_pitch;
            dest_rows_i += dest_pitch;
        }
    }
}


static void v_draw_fill(const bam_rect_t* dest_rect, bam_color_t color, void* user_data) {
    SDL_Rect r;

    // unused arguments
    (void) user_data;

    // draw filled in rectangle on tile surface using SDL library
    r.x = dest_rect->x1;
    r.y = dest_rect->y1;
    r.w = dest_rect->x2 - dest_rect->x1;
    r.h = dest_rect->y2 - dest_rect->y1;

    SDL_FillRect(m_tile, &r, color);
}


static void v_blt_tile(int x, int y, void* user_data) {
    SDL_Rect src_rect;
    SDL_Rect dest_rect;

    // unused arguments
    (void) user_data;

    // copy tile surface to display surface at specified position
    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.w = APP_TILE_WIDTH;
    src_rect.h = APP_TILE_HEIGHT;

    dest_rect.x = x;
    dest_rect.y = y;
    dest_rect.w = APP_TILE_WIDTH;
    dest_rect.h = APP_TILE_HEIGHT;

    SDL_BlitSurface(m_tile, &src_rect, m_surface, &dest_rect);

    // flag window surface as requiring update (handled in v_get_event)
    m_update_surface = true;
}


// ******** EXECUTION ENTRY POINT ********

int main(int argc, char* argv[]) {
    static const bam_vtable_t VTABLE = {
            .panic = v_panic,
            .get_monotonic_time = v_get_monotonic_time,
            .get_event = v_get_event,
            .get_font_metrics = v_get_font_metrics,
            .get_glyph_metrics = v_get_glyph_metrics,
            .draw_glyph = v_draw_glyph,
            .draw_fill = v_draw_fill,
            .blt_tile = v_blt_tile
    };

    static uint32_t dirty_buffer[APP_DIRTY_BUFFER_SIZE];
    static bam_widget_t widget_buffer[APP_WIDGET_BUFFER_SIZE];

    int exit_code = EXIT_FAILURE;

    // unused arguments
    (void) argc;
    (void) argv;

    // initialise SDL library
    SDL_SetMainReady();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        goto cleanup1;
    }

    // create window
    m_window = SDL_CreateWindow(
            "BaM SDL2 Demo",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            APP_DISPLAY_WIDTH,
            APP_DISPLAY_HEIGHT,
            SDL_WINDOW_SHOWN);

    if (!m_window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        goto cleanup2;
    }

    // get window surface
    m_surface = SDL_GetWindowSurface(m_window);

    if (!m_surface) {
        fprintf(stderr, "SDL_GetWindowSurface: %s\n", SDL_GetError());
        goto cleanup3;
    }

    // create back buffer tile surface
    m_tile = SDL_CreateRGBSurfaceWithFormat(
            0,
            APP_TILE_WIDTH,
            APP_TILE_HEIGHT,
            32,
            SDL_PIXELFORMAT_RGBA32
    );

    if (!m_tile) {
        fprintf(stderr, "SDL_CreateRGBSurfaceWithFormat: %s\n", SDL_GetError());
        goto cleanup3;
    }

    // set long jmp for v_panic to use
    if (setjmp(m_panic_jmp)) {
        // execution will jump here if BaM context panics
        goto cleanup4;
    }

    // initialise BaM context
    bam_init(
            &m_bam,
            dirty_buffer,
            APP_DIRTY_BUFFER_SIZE,
            widget_buffer,
            APP_WIDGET_BUFFER_SIZE,
            APP_DISPLAY_WIDTH,
            APP_DISPLAY_HEIGHT,
            APP_TILE_WIDTH,
            APP_TILE_HEIGHT,
            0xFF101010ul,
            &APP_DEFAULT_STYLE,
            &VTABLE,
            NULL);

    // start event loop
    //bam_start(&m_bam);
    double value = 1000.0;

    if ( bam_edit_real(&m_bam, &value, &APP_EDITOR_STYLE) ) {
        printf("ACCEPTED: %f\n", value);
    } else {
        printf("CANCELLED\n");
    }

    // cleanup and exit
    exit_code = EXIT_SUCCESS;

cleanup4:

    // destroy tile surface
    SDL_FreeSurface(m_tile);


cleanup3:

    // destroy window
    SDL_DestroyWindow(m_window);

cleanup2:

    // finalize SDL library
    SDL_Quit();

cleanup1:

    return exit_code;
}
