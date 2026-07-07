#include "wayland.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

static int allocate_shm_file(size_t size)
{
    int fd = memfd_create("dbd-timer-shm", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd >= 0) {
        if (ftruncate(fd, (off_t)size) < 0) { close(fd); return -1; }
        return fd;
    }
    const char *dir = getenv("XDG_RUNTIME_DIR");
    if (!dir) dir = "/tmp";
    char path[256];
    snprintf(path, sizeof(path), "%s/dbd-timer-shm-XXXXXX", dir);
    fd = mkstemp(path);
    if (fd < 0) return -1;
    unlink(path);
    if (ftruncate(fd, (off_t)size) < 0) { close(fd); return -1; }
    return fd;
}

static void *create_shm_buffer(struct wl_shm *shm, int width, int height,
    struct wl_buffer **out_buffer, int *out_size)
{
    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
    int size = stride * height;
    int fd = allocate_shm_file(size);
    if (fd < 0) return NULL;

    void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) { close(fd); return NULL; }

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    *out_buffer = wl_shm_pool_create_buffer(pool, 0, width, height,
        stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
    *out_size = size;
    return data;
}

static void registry_global(void *data, struct wl_registry *registry,
    uint32_t name, const char *interface, uint32_t version)
{
    struct wayland_state *wl = data;
    if (strcmp(interface, wl_compositor_interface.name) == 0)
        wl->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    else if (strcmp(interface, wl_shm_interface.name) == 0)
        wl->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0)
        wl->layer_shell = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 4);
    else if (strcmp(interface, xdg_wm_base_interface.name) == 0)
        wl->wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
}

static const struct wl_registry_listener registry_listener = {
    registry_global, NULL
};

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *wm_base, uint32_t serial)
{
    xdg_wm_base_pong(wm_base, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = { xdg_wm_base_ping };

static void layer_surface_configure(void *data, struct zwlr_layer_surface_v1 *surface,
    uint32_t serial, uint32_t width, uint32_t height)
{
    struct wayland_state *wl = data;
    wl->overlay.configured = 1;
    wl->overlay.configure_serial = serial;
    if (width > 0 && height > 0) {
        wl->width = width;
        wl->height = height;
    }
}

static void layer_surface_closed(void *data, struct zwlr_layer_surface_v1 *surface)
{
    (void)data; (void)surface;
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    layer_surface_configure, layer_surface_closed
};

static void xdg_surface_configure(void *data, struct xdg_surface *surface, uint32_t serial)
{
    struct wayland_state *wl = data;
    wl->companion.configured = 1;
    wl->companion.configure_serial = serial;
    xdg_surface_ack_configure(surface, serial);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    xdg_surface_configure
};

static void toplevel_configure(void *data, struct xdg_toplevel *toplevel,
    int32_t width, int32_t height, struct wl_array *states)
{
    (void)data; (void)toplevel; (void)states; (void)width; (void)height;
}

static void toplevel_close(void *data, struct xdg_toplevel *toplevel)
{
    (void)data; (void)toplevel;
}

static const struct xdg_toplevel_listener toplevel_listener = {
    toplevel_configure, toplevel_close
};

static void set_empty_input_region(struct wl_compositor *compositor, struct wl_surface *surface)
{
    struct wl_region *empty = wl_compositor_create_region(compositor);
    wl_surface_set_input_region(surface, empty);
    wl_region_destroy(empty);
}

static int init_overlay(struct wayland_state *wl)
{
    wl->overlay.surface = wl_compositor_create_surface(wl->compositor);
    if (!wl->overlay.surface) return -1;

    wl->overlay.layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        wl->layer_shell, wl->overlay.surface, NULL,
        ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "dbd-timer");
    if (!wl->overlay.layer_surface) return -1;

    zwlr_layer_surface_v1_add_listener(wl->overlay.layer_surface,
        &layer_surface_listener, wl);

    zwlr_layer_surface_v1_set_anchor(wl->overlay.layer_surface,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
    zwlr_layer_surface_v1_set_size(wl->overlay.layer_surface, wl->width, wl->height);
    zwlr_layer_surface_v1_set_margin(wl->overlay.layer_surface, 10, 0, 0, 10);
    zwlr_layer_surface_v1_set_keyboard_interactivity(wl->overlay.layer_surface,
        ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
    zwlr_layer_surface_v1_set_exclusive_zone(wl->overlay.layer_surface, -1);

    set_empty_input_region(wl->compositor, wl->overlay.surface);

    wl_surface_commit(wl->overlay.surface);
    wl_display_roundtrip(wl->display);

    if (!wl->overlay.configured) {
        fprintf(stderr, "dbd-timer: overlay not configured\n");
        return -1;
    }

    zwlr_layer_surface_v1_ack_configure(wl->overlay.layer_surface,
        wl->overlay.configure_serial);
    return 0;
}

static int init_companion(struct wayland_state *wl)
{
    wl->companion.buffer = NULL;
    wl->companion.shm_data = NULL;

    wl->companion.surface = wl_compositor_create_surface(wl->compositor);
    if (!wl->companion.surface) return -1;

    wl->companion.xdg_surface = xdg_wm_base_get_xdg_surface(
        wl->wm_base, wl->companion.surface);
    if (!wl->companion.xdg_surface) return -1;
    xdg_surface_add_listener(wl->companion.xdg_surface, &xdg_surface_listener, wl);

    wl->companion.toplevel = xdg_surface_get_toplevel(wl->companion.xdg_surface);

    xdg_toplevel_add_listener(wl->companion.toplevel, &toplevel_listener, wl);
    xdg_toplevel_set_title(wl->companion.toplevel, "DBD Timer");
    xdg_toplevel_set_app_id(wl->companion.toplevel, "dbd-timer");

    set_empty_input_region(wl->compositor, wl->companion.surface);

    wl_surface_commit(wl->companion.surface);
    wl_display_roundtrip(wl->display);

    if (!wl->companion.configured) return -1;

    wl->companion.shm_data = create_shm_buffer(wl->shm, 1, 1,
        &wl->companion.buffer, &wl->companion.buffer_size);

    if (!wl->companion.shm_data) return -1;

    memset(wl->companion.shm_data, 0, wl->companion.buffer_size);

    wl_surface_attach(wl->companion.surface, wl->companion.buffer, 0, 0);
    wl_surface_damage_buffer(wl->companion.surface, 0, 0, 1, 1);
    wl_surface_commit(wl->companion.surface);
    wl_display_flush(wl->display);

    return 0;
}

int wayland_init(struct wayland_state *wl)
{
    memset(wl, 0, sizeof(*wl));
    wl->width = 320;
    wl->height = 96;

    wl->display = wl_display_connect(NULL);
    if (!wl->display) {
        fprintf(stderr, "Failed to connect to Wayland display\n");
        return -1;
    }

    wl->registry = wl_display_get_registry(wl->display);
    wl_registry_add_listener(wl->registry, &registry_listener, wl);
    wl_display_roundtrip(wl->display);

    if (!wl->compositor || !wl->shm) {
        fprintf(stderr, "Missing wl_compositor or wl_shm\n");
        return -1;
    }
    if (!wl->layer_shell) {
        fprintf(stderr, "Missing wlr-layer-shell (not supported by compositor)\n");
        return -1;
    }

    if (wl->wm_base)
        xdg_wm_base_add_listener(wl->wm_base, &wm_base_listener, wl);

    if (init_overlay(wl) < 0) return -1;
    fprintf(stderr, "dbd-timer: overlay created\n");

    if (wl->wm_base) {
        if (init_companion(wl) == 0)
            fprintf(stderr, "dbd-timer: companion window in taskbar\n");
        else
            fprintf(stderr, "dbd-timer: companion not available\n");
    }

    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, wl->width);
    wl->buffer_size = stride * wl->height;
    wl->shm_data = create_shm_buffer(wl->shm, wl->width, wl->height,
        &wl->buffer, &wl->buffer_size);
    if (!wl->shm_data) {
        fprintf(stderr, "Failed to create main buffer\n");
        return -1;
    }

    render_init(&wl->render, wl->width, wl->height, wl->shm_data, stride);

    wl_surface_attach(wl->overlay.surface, wl->buffer, 0, 0);
    wl_surface_damage_buffer(wl->overlay.surface, 0, 0, wl->width, wl->height);
    wl_surface_commit(wl->overlay.surface);
    wl_display_flush(wl->display);

    return 0;
}

void wayland_commit(struct wayland_state *wl)
{
    wl_surface_attach(wl->overlay.surface, wl->buffer, 0, 0);
    wl_surface_damage_buffer(wl->overlay.surface, 0, 0, wl->width, wl->height);
    wl_surface_commit(wl->overlay.surface);
    wl_display_flush(wl->display);
}

void wayland_destroy(struct wayland_state *wl)
{
    render_destroy(&wl->render);

    if (wl->buffer && wl->buffer_size > 0) {
        munmap(wl->shm_data, wl->buffer_size);
        wl_buffer_destroy(wl->buffer);
    }
    if (wl->companion.buffer && wl->companion.buffer_size > 0) {
        munmap(wl->companion.shm_data, wl->companion.buffer_size);
        wl_buffer_destroy(wl->companion.buffer);
    }

    if (wl->overlay.layer_surface) zwlr_layer_surface_v1_destroy(wl->overlay.layer_surface);
    if (wl->overlay.surface) wl_surface_destroy(wl->overlay.surface);
    if (wl->companion.toplevel) xdg_toplevel_destroy(wl->companion.toplevel);
    if (wl->companion.xdg_surface) xdg_surface_destroy(wl->companion.xdg_surface);
    if (wl->companion.surface) wl_surface_destroy(wl->companion.surface);
    if (wl->layer_shell) zwlr_layer_shell_v1_destroy(wl->layer_shell);
    if (wl->wm_base) xdg_wm_base_destroy(wl->wm_base);
    if (wl->compositor) wl_compositor_destroy(wl->compositor);
    if (wl->shm) wl_shm_destroy(wl->shm);
    if (wl->registry) wl_registry_destroy(wl->registry);
    if (wl->display) wl_display_disconnect(wl->display);
}
