// Main loop: Wayland event loop with a poll()-based wakeup pipe for input commands.
// Renders the overlay whenever the timer state or display text changes.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#include "wayland.h"
#include "timer.h"
#include "config.h"
#include "input.h"
#include "render.h"

// App state — owns the Wayland connection, timer logic, input threads, and text cache.
struct app {
    struct wayland_state wl;
    struct timer_state timer;
    struct input_state input;

    int wakeup_pipe[2];
    volatile int stop;

    char text[2][16];
    char prev_text[2][16];
    int prev_active;
    int prev_running;
};

static struct app *g_app;

static void handle_signal(int sig)
{
    (void)sig;
    if (g_app) g_app->stop = 1;
}

// Render elapsed time as ss:cs (under 1 min) or mm:ss:cs (at or over 1 min).
// cs = centiseconds (2 digits), no leading zero on the first field.
static void format_time(char *buf, size_t len, uint64_t elapsed_ns)
{
    uint64_t total_cs = elapsed_ns / 10000000ULL;
    uint64_t mm = total_cs / 6000;
    uint64_t ss = (total_cs / 100) % 60;
    uint64_t cs = total_cs % 100;

    if (mm > 0) {
        snprintf(buf, len, "%lu:%02lu:%02lu", mm, ss, cs);
    } else {
        snprintf(buf, len, "%lu:%02lu", ss, cs);
    }
}

// Returns 1 if the Wayland overlay should be redrawn (active timer changed,
// running state changed, or display text changed).
static int needs_update(struct app *app)
{
    int active = timer_get_active(&app->timer);
    int running = app->timer.timers[active].running;

    if (active != app->prev_active) return 1;
    if (running != app->prev_running) return 1;

    for (int i = 0; i < 2; i++) {
        uint64_t ns = timer_get_elapsed_ns(&app->timer, i);
        format_time(app->text[i], sizeof(app->text[i]), ns);
        if (strcmp(app->text[i], app->prev_text[i]) != 0) return 1;
    }
    return 0;
}

// Dispatch a command byte from the wakeup pipe to the timer state machine.
// CMD_TOGGLE is state-aware: running→stop, stopped-with-time→reset, zero→start.
static void handle_cmd(struct app *app, int cmd)
{
    switch (cmd) {
    case CMD_SEL1:
        timer_select(&app->timer, 0);
        fprintf(stderr, "dbd-timer: selected T1\n");
        break;
    case CMD_SEL2:
        timer_select(&app->timer, 1);
        fprintf(stderr, "dbd-timer: selected T2\n");
        break;
    case CMD_TOGGLE: {
        int idx = timer_get_active(&app->timer);
        if (app->timer.timers[idx].running) {
            timer_toggle(&app->timer);
            fprintf(stderr, "dbd-timer: toggle T%d STOP\n", idx + 1);
        } else if (app->timer.timers[idx].accumulated_ns > 0) {
            timer_reset_one(&app->timer, idx);
            fprintf(stderr, "dbd-timer: reset T%d\n", idx + 1);
        } else {
            timer_toggle(&app->timer);
            fprintf(stderr, "dbd-timer: toggle T%d START\n", idx + 1);
        }
        break;
    }
    case CMD_QUIT:
        fprintf(stderr, "dbd-timer: quit\n");
        app->stop = 1;
        break;
    }
}

// Snapshot current timer state into the text cache and push a render to the overlay.
static void render_frame(struct app *app)
{
    int active = timer_get_active(&app->timer);
    int running = app->timer.timers[active].running;

    for (int i = 0; i < 2; i++) {
        strncpy(app->prev_text[i], app->text[i], sizeof(app->prev_text[i]));
    }
    app->prev_active = active;
    app->prev_running = running;

    char display1[32], display2[32];
    snprintf(display1, sizeof(display1), "%s", app->text[0]);
    snprintf(display2, sizeof(display2), "%s", app->text[1]);

    render_draw(&app->wl.render, display1, display2, active, running);
    wayland_commit(&app->wl);
}

// Entry point: init Wayland → timer → config → input, then run the poll loop.
int main(void)
{
    struct app app;
    memset(&app, 0, sizeof(app));
    g_app = &app;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGPIPE, SIG_IGN);

    fprintf(stderr, "dbd-timer: starting...\n");

    if (pipe2(app.wakeup_pipe, O_CLOEXEC | O_NONBLOCK) < 0) {
        perror("dbd-timer: pipe2");
        return 1;
    }

    if (wayland_init(&app.wl) < 0) {
        fprintf(stderr, "dbd-timer: Wayland init failed\n");
        return 1;
    }
    fprintf(stderr, "dbd-timer: Wayland overlay created\n");

    timer_init(&app.timer);

    for (int i = 0; i < 2; i++) {
        app.text[i][0] = '\0';
        app.prev_text[i][0] = '\0';
    }
    app.prev_active = -1;
    app.prev_running = -1;

    struct keybinds kb;
    config_load(&kb);

    input_init(&app.input, app.wakeup_pipe[1], &kb);

    render_frame(&app);

    struct pollfd fds[2];
    fds[0].fd = wl_display_get_fd(app.wl.display);
    fds[0].events = POLLIN;
    fds[1].fd = app.wakeup_pipe[0];
    fds[1].events = POLLIN;

    while (!app.stop) {
        while (wl_display_prepare_read(app.wl.display) < 0)
            wl_display_dispatch_pending(app.wl.display);

        wl_display_flush(app.wl.display);

        int ret = poll(fds, 2, 50);
        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("dbd-timer: poll");
            break;
        }

        if (fds[1].revents & POLLIN) {
            char buf[64];
            ssize_t n = read(app.wakeup_pipe[0], buf, sizeof(buf));
            for (ssize_t i = 0; i < n; i++) {
                handle_cmd(&app, buf[i]);
                if (app.stop) break;
            }
        }

        wl_display_read_events(app.wl.display);
        wl_display_dispatch_pending(app.wl.display);

        if (needs_update(&app)) {
            render_frame(&app);
        }
    }

    input_destroy(&app.input);
    wayland_destroy(&app.wl);
    close(app.wakeup_pipe[0]);
    close(app.wakeup_pipe[1]);

    return 0;
}
