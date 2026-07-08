// Key binding config: parses ~/.config/dbd-timer/config into key code arrays.

#pragma once

#include <linux/input.h>

#define MAX_KEYS_PER_CMD 8

#define MOD_NONE    0
#define MOD_CTRL    1
#define MOD_SHIFT   2
#define MOD_ALT     4

struct keybinds {
    int n_select1;
    int keys_select1[MAX_KEYS_PER_CMD];
    int mods_select1[MAX_KEYS_PER_CMD];
    int n_select2;
    int keys_select2[MAX_KEYS_PER_CMD];
    int mods_select2[MAX_KEYS_PER_CMD];
    int n_toggle;
    int keys_toggle[MAX_KEYS_PER_CMD];
    int mods_toggle[MAX_KEYS_PER_CMD];
    int n_quit;
    int keys_quit[MAX_KEYS_PER_CMD];
    int mods_quit[MAX_KEYS_PER_CMD];
};

void config_init(struct keybinds *kb);
void config_load(struct keybinds *kb);
int  keybinds_lookup(const struct keybinds *kb, int key_code, int mods);
