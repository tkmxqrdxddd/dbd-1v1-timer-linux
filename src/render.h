// Cairo+Pango rendering on a shared-memory buffer for the Wayland overlay.

#pragma once

#include <cairo.h>
#include <pango/pangocairo.h>

struct render_state {
    cairo_surface_t *surface;
    cairo_t *cr;
    PangoLayout *layout;
    int width;
    int height;
    int stride;
    void *shm_data;
    cairo_font_options_t *font_opts;
};

void render_init(struct render_state *rs, int width, int height, void *shm_data, int stride);
void render_destroy(struct render_state *rs);
void render_draw(struct render_state *rs, const char *text1, const char *text2, int active_idx, int running);
