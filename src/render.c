// Transparent overlay renderer: dark 50% background, two timer lines
// (labels T1/T2 in green or grey, time text in white), green dot when running.

#include "render.h"
#include <string.h>

// Set up a cairo surface backed by an SHM buffer and a shared Pango layout (monospace 28 bold).
void render_init(struct render_state *rs, int width, int height, void *shm_data, int stride)
{
    rs->width = width;
    rs->height = height;
    rs->shm_data = shm_data;
    rs->stride = stride;

    rs->surface = cairo_image_surface_create_for_data(
        shm_data, CAIRO_FORMAT_ARGB32, width, height, stride);
    rs->cr = cairo_create(rs->surface);
    rs->layout = pango_cairo_create_layout(rs->cr);

    PangoFontDescription *desc = pango_font_description_new();
    pango_font_description_set_family(desc, "monospace");
    pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
    pango_font_description_set_absolute_size(desc, 28 * PANGO_SCALE);
    pango_layout_set_font_description(rs->layout, desc);
    pango_font_description_free(desc);

    rs->font_opts = cairo_font_options_create();
    cairo_font_options_set_antialias(rs->font_opts, CAIRO_ANTIALIAS_SUBPIXEL);
}

// Tear down cairo surface, context, and Pango layout.
void render_destroy(struct render_state *rs)
{
    if (rs->layout) g_object_unref(rs->layout);
    if (rs->cr) cairo_destroy(rs->cr);
    if (rs->surface) cairo_surface_destroy(rs->surface);
    if (rs->font_opts) cairo_font_options_destroy(rs->font_opts);
}

// Draw a single frame: clear → dark background → T1 label → T2 label → time text → green dot.
// text1/text2 are pre-formatted by format_time() in main.c.
void render_draw(struct render_state *rs, const char *text1, const char *text2, int active_idx, int running)
{
    cairo_t *cr = rs->cr;

    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.0);
    cairo_paint(cr);

    cairo_set_source_rgba(cr, 0, 0, 0, 0.5);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_rectangle(cr, 0, 0, rs->width, rs->height);
    cairo_fill(cr);

    int y1 = 10;
    int y2 = 48;

    cairo_set_font_options(cr, rs->font_opts);
    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 24);

    if (active_idx == 0) {
        cairo_set_source_rgba(cr, 0.3, 0.8, 0.3, 1.0);
    } else {
        cairo_set_source_rgba(cr, 0.7, 0.7, 0.7, 0.6);
    }
    pango_layout_set_text(rs->layout, "T1", -1);
    pango_cairo_update_layout(cr, rs->layout);
    cairo_move_to(cr, 12, y1);
    pango_cairo_show_layout(cr, rs->layout);

    if (active_idx == 1) {
        cairo_set_source_rgba(cr, 0.3, 0.8, 0.3, 1.0);
    } else {
        cairo_set_source_rgba(cr, 0.7, 0.7, 0.7, 0.6);
    }
    pango_layout_set_text(rs->layout, "T2", -1);
    pango_cairo_update_layout(cr, rs->layout);
    cairo_move_to(cr, 12, y2);
    pango_cairo_show_layout(cr, rs->layout);

    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.95);
    cairo_move_to(cr, 70, y1);
    pango_layout_set_text(rs->layout, text1, -1);
    pango_cairo_update_layout(cr, rs->layout);
    pango_cairo_show_layout(cr, rs->layout);

    cairo_move_to(cr, 70, y2);
    pango_layout_set_text(rs->layout, text2, -1);
    pango_cairo_update_layout(cr, rs->layout);
    pango_cairo_show_layout(cr, rs->layout);

    if (running) {
        cairo_set_source_rgba(cr, 0.2, 1.0, 0.2, 1.0);
        cairo_arc(cr, 5, active_idx == 0 ? (y1 + 16) : (y2 + 16), 4, 0, 2 * 3.14159);
        cairo_fill(cr);
    }

    cairo_surface_flush(rs->surface);
}
