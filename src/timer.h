#pragma once

#include <stdint.h>

struct timer_state {
    int active_idx;
    struct {
        int running;
        uint64_t start_time_ns;
        uint64_t accumulated_ns;
    } timers[2];
};

void timer_init(struct timer_state *ts);
void timer_select(struct timer_state *ts, int idx);
void timer_toggle(struct timer_state *ts);
void timer_reset(struct timer_state *ts);
int timer_get_active(struct timer_state *ts);
void timer_reset_one(struct timer_state *ts, int idx);
uint64_t timer_get_elapsed_ns(struct timer_state *ts, int idx);
