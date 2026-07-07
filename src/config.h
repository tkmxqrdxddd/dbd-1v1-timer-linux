#pragma once

#include <linux/input.h>

#define MAX_KEYS_PER_CMD 8

struct keybinds {
    int n_select1;
    int keys_select1[MAX_KEYS_PER_CMD];
    int n_select2;
    int keys_select2[MAX_KEYS_PER_CMD];
    int n_toggle;
    int keys_toggle[MAX_KEYS_PER_CMD];
    int n_quit;
    int keys_quit[MAX_KEYS_PER_CMD];
};

void config_init(struct keybinds *kb);
void config_load(struct keybinds *kb);
int  keybinds_lookup(const struct keybinds *kb, int key_code);
