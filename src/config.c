#include "config.h"
#include "input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define KEY_ENTRY(n) {#n, n}

static const struct { const char *name; int code; } key_map[] = {
    KEY_ENTRY(KEY_ESC),
    KEY_ENTRY(KEY_F1),
    KEY_ENTRY(KEY_F2),
    KEY_ENTRY(KEY_F),
    KEY_ENTRY(KEY_LEFT),
    KEY_ENTRY(KEY_RIGHT),
    KEY_ENTRY(KEY_UP),
    KEY_ENTRY(KEY_DOWN),
    KEY_ENTRY(KEY_ENTER),
    KEY_ENTRY(KEY_SPACE),
    KEY_ENTRY(KEY_TAB),
    KEY_ENTRY(KEY_BACKSPACE),
    KEY_ENTRY(BTN_LEFT),
    KEY_ENTRY(BTN_RIGHT),
    KEY_ENTRY(BTN_MIDDLE),
    KEY_ENTRY(BTN_SIDE),
    KEY_ENTRY(BTN_EXTRA),
    KEY_ENTRY(BTN_FORWARD),
    KEY_ENTRY(BTN_BACK),
    KEY_ENTRY(BTN_SOUTH),
    KEY_ENTRY(BTN_EAST),
    KEY_ENTRY(BTN_NORTH),
    KEY_ENTRY(BTN_WEST),
    KEY_ENTRY(BTN_TL),
    KEY_ENTRY(BTN_TR),
    KEY_ENTRY(BTN_TL2),
    KEY_ENTRY(BTN_TR2),
    KEY_ENTRY(BTN_SELECT),
    KEY_ENTRY(BTN_START),
    KEY_ENTRY(BTN_MODE),
    KEY_ENTRY(BTN_THUMB),
    KEY_ENTRY(BTN_THUMB2),
    {NULL, 0},
};

void config_init(struct keybinds *kb)
{
    memset(kb, 0, sizeof(*kb));
    kb->n_select1 = 1; kb->keys_select1[0] = KEY_F1;
    kb->n_select2 = 1; kb->keys_select2[0] = KEY_F2;
    kb->n_toggle  = 2; kb->keys_toggle[0]  = KEY_F;
                        kb->keys_toggle[1]  = BTN_SIDE;
    kb->n_quit    = 1; kb->keys_quit[0]    = KEY_ESC;
}

static int parse_key(const char *s)
{
    for (int i = 0; key_map[i].name; i++)
        if (strcasecmp(s, key_map[i].name) == 0)
            return key_map[i].code;
    return -1;
}

static char *trim(char *s)
{
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)*(end - 1))) end--;
    *end = '\0';
    return s;
}

static char *config_path(void)
{
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) {
        char *path = NULL;
        int n = asprintf(&path, "%s/dbd-timer/config", xdg);
        if (n > 0 && path) return path;
        free(path);
    }
    const char *home = getenv("HOME");
    if (home) {
        char *path = NULL;
        int n = asprintf(&path, "%s/.config/dbd-timer/config", home);
        if (n > 0 && path) return path;
        free(path);
    }
    return NULL;
}

static void add_key(struct keybinds *kb, const char *cmd, int code)
{
    if (code < 0) return;
    if (strcmp(cmd, "select1") == 0 && kb->n_select1 < MAX_KEYS_PER_CMD)
        kb->keys_select1[kb->n_select1++] = code;
    else if (strcmp(cmd, "select2") == 0 && kb->n_select2 < MAX_KEYS_PER_CMD)
        kb->keys_select2[kb->n_select2++] = code;
    else if (strcmp(cmd, "toggle") == 0 && kb->n_toggle < MAX_KEYS_PER_CMD)
        kb->keys_toggle[kb->n_toggle++] = code;
    else if (strcmp(cmd, "quit") == 0 && kb->n_quit < MAX_KEYS_PER_CMD)
        kb->keys_quit[kb->n_quit++] = code;
}

void config_load(struct keybinds *kb)
{
    config_init(kb);

    char *path = config_path();
    if (!path) return;

    FILE *f = fopen(path, "r");
    free(path);
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *s = trim(line);
        if (*s == '#' || *s == '\0') continue;

        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = '\0';
        char *cmd = trim(s);
        char *vals = trim(eq + 1);

        char *token = strtok(vals, ",");
        while (token) {
            char *t = trim(token);
            if (*t) {
                int code = parse_key(t);
                if (code < 0) {
                    fprintf(stderr, "dbd-timer: unknown key '%s' in config\n", t);
                } else {
                    add_key(kb, cmd, code);
                }
            }
            token = strtok(NULL, ",");
        }
    }
    fclose(f);
}

int keybinds_lookup(const struct keybinds *kb, int key_code)
{
    for (int i = 0; i < kb->n_select1; i++)
        if (kb->keys_select1[i] == key_code) return CMD_SEL1;
    for (int i = 0; i < kb->n_select2; i++)
        if (kb->keys_select2[i] == key_code) return CMD_SEL2;
    for (int i = 0; i < kb->n_toggle; i++)
        if (kb->keys_toggle[i] == key_code) return CMD_TOGGLE;
    for (int i = 0; i < kb->n_quit; i++)
        if (kb->keys_quit[i] == key_code) return CMD_QUIT;
    return CMD_NONE;
}
