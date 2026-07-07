#include "timer.h"
#include <time.h>

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

void timer_init(struct timer_state *ts)
{
    ts->active_idx = 0;
    for (int i = 0; i < 2; i++) {
        ts->timers[i].running = 0;
        ts->timers[i].start_time_ns = 0;
        ts->timers[i].accumulated_ns = 0;
    }
}

void timer_select(struct timer_state *ts, int idx)
{
    if (idx < 0 || idx > 1) return;
    if (ts->timers[ts->active_idx].running) {
        uint64_t now = now_ns();
        ts->timers[ts->active_idx].accumulated_ns +=
            now - ts->timers[ts->active_idx].start_time_ns;
        ts->timers[ts->active_idx].running = 0;
    }
    ts->active_idx = idx;
}

void timer_toggle(struct timer_state *ts)
{
    int idx = ts->active_idx;
    if (ts->timers[idx].running) {
        uint64_t now = now_ns();
        ts->timers[idx].accumulated_ns +=
            now - ts->timers[idx].start_time_ns;
        ts->timers[idx].running = 0;
    } else {
        ts->timers[idx].start_time_ns = now_ns();
        ts->timers[idx].running = 1;
    }
}

void timer_reset(struct timer_state *ts)
{
    for (int i = 0; i < 2; i++) {
        ts->timers[i].running = 0;
        ts->timers[i].accumulated_ns = 0;
    }
}

void timer_reset_one(struct timer_state *ts, int idx)
{
    if (idx < 0 || idx > 1) return;
    ts->timers[idx].running = 0;
    ts->timers[idx].accumulated_ns = 0;
}

int timer_get_active(struct timer_state *ts)
{
    return ts->active_idx;
}

uint64_t timer_get_elapsed_ns(struct timer_state *ts, int idx)
{
    if (ts->timers[idx].running) {
        uint64_t now = now_ns();
        return ts->timers[idx].accumulated_ns +
            (now - ts->timers[idx].start_time_ns);
    }
    return ts->timers[idx].accumulated_ns;
}
