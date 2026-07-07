#pragma once

#include <wayland-client.h>
#include "layer-protocol.h"
#include "xdg-protocol.h"
#include "render.h"

struct overlay_window {
    struct wl_surface *surface;
    struct zwlr_layer_surface_v1 *layer_surface;
    int configured;
    uint32_t configure_serial;
};

struct companion_window {
    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *toplevel;
    struct wl_buffer *buffer;
    void *shm_data;
    int buffer_size;
    int configured;
    uint32_t configure_serial;
};

struct wayland_state {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct zwlr_layer_shell_v1 *layer_shell;
    struct xdg_wm_base *wm_base;

    struct overlay_window overlay;
    struct companion_window companion;

    int width;
    int height;

    struct wl_buffer *buffer;
    int buffer_size;
    void *shm_data;

    struct render_state render;
};

int wayland_init(struct wayland_state *wl);
void wayland_destroy(struct wayland_state *wl);
void wayland_commit(struct wayland_state *wl);
