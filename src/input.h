// Input subsystem: evdev thread (keyboard/mouse) + SDL thread (gamepad).
// Commands are written as single bytes to a wakeup pipe consumed by main.

#pragma once

#include "config.h"

#define CMD_NONE      0
#define CMD_SEL1      1
#define CMD_SEL2      2
#define CMD_TOGGLE    3
#define CMD_QUIT      5

struct input_state {
    struct libevdev_state *evdev;
    struct sdl_state *sdl;
    int wakeup_fd;
};

int input_init(struct input_state *is, int wakeup_fd, const struct keybinds *kb);
void input_destroy(struct input_state *is);
