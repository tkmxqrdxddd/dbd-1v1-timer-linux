// Two parallel input threads:
//   evdev_thread — scans /dev/input/event* for devices with bound keys, polls with poll()
//   sdl_thread   — SDL gamecontroller loop, handles hotplug via SDL_CONTROLLERDEVICEADDED
// Both write a single-byte command to the wakeup pipe on each relevant key press.

#include "input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>
#include <libevdev/libevdev.h>
#include <SDL2/SDL.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>

#define MAX_DEVICES 32

struct libevdev_state {
    pthread_t thread;
    int stop;
    int wakeup_fd;
    int mods;  // currently held modifier keys (MOD_* bitmask)
    struct keybinds kb;
};

struct sdl_state {
    pthread_t thread;
    int stop;
    int wakeup_fd;
};

// Write a one-byte command to the wakeup pipe consumed by the main loop.
static void send_cmd(int fd, int cmd)
{
    (void)write(fd, &cmd, 1);
}

// Convert an evdev key code to a modifier bit, or 0 if it's not a modifier.
static int code_to_mod(int code)
{
    switch (code) {
    case KEY_LEFTCTRL:  case KEY_RIGHTCTRL:  return MOD_CTRL;
    case KEY_LEFTSHIFT: case KEY_RIGHTSHIFT: return MOD_SHIFT;
    case KEY_LEFTALT:   case KEY_RIGHTALT:   return MOD_ALT;
    default: return 0;
    }
}

// True if the evdev device exposes any of the key codes listed in kb.
// Replaces the old is_real_keyboard() — also matches mice that have BTN_SIDE etc.
static int device_has_bindings(struct libevdev *dev, const struct keybinds *kb)
{
    if (!libevdev_has_event_type(dev, EV_KEY))
        return 0;
    for (int i = 0; i < kb->n_select1; i++)
        if (libevdev_has_event_code(dev, EV_KEY, kb->keys_select1[i]))
            return 1;
    for (int i = 0; i < kb->n_select2; i++)
        if (libevdev_has_event_code(dev, EV_KEY, kb->keys_select2[i]))
            return 1;
    for (int i = 0; i < kb->n_toggle; i++)
        if (libevdev_has_event_code(dev, EV_KEY, kb->keys_toggle[i]))
            return 1;
    for (int i = 0; i < kb->n_quit; i++)
        if (libevdev_has_event_code(dev, EV_KEY, kb->keys_quit[i]))
            return 1;
    return 0;
}

// Scans /dev/input for devices with bound keys, then polls for EV_KEY 1->0→1 events.
// Re-scans every time a device error occurs so hotplug works transparently.
static void *evdev_thread(void *arg)
{
    struct libevdev_state *state = arg;
    int warned_group = 0;
    int warned_no_device = 0;

    while (!state->stop) {
        struct libevdev *devices[MAX_DEVICES];
        int device_fds[MAX_DEVICES];
        int device_count = 0;

        DIR *dir = opendir("/dev/input");
        if (!dir) {
            fprintf(stderr, "dbd-timer: /dev/input: %s\n", strerror(errno));
            sleep(5);
            continue;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL && device_count < MAX_DEVICES) {
            if (strncmp(entry->d_name, "event", 5) != 0) continue;

            char path[512];
            snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);
            int fd = open(path, O_RDONLY | O_NONBLOCK);
            if (fd < 0) {
                if (!warned_group && errno == EACCES) {
                    fprintf(stderr, "dbd-timer: permission denied on %s\n", path);
                    fprintf(stderr, "dbd-timer: run: sudo usermod -aG input $USER\n");
                    warned_group = 1;
                }
                continue;
            }

            struct libevdev *dev = NULL;
            if (libevdev_new_from_fd(fd, &dev) < 0) {
                close(fd);
                continue;
            }

            if (device_has_bindings(dev, &state->kb)) {
                devices[device_count] = dev;
                device_fds[device_count] = fd;
                device_count++;
                fprintf(stderr, "dbd-timer: device %d = %s (%s)\n",
                    device_count, entry->d_name, libevdev_get_name(dev));
            } else {
                libevdev_free(dev);
                close(fd);
            }
        }
        closedir(dir);

        if (device_count == 0) {
            if (!warned_no_device) {
                fprintf(stderr, "dbd-timer: no input device with bound keys found\n");
                warned_no_device = 1;
            }
            sleep(2);
            continue;
        }

        warned_no_device = 0;

        struct pollfd pfds[MAX_DEVICES];
        for (int i = 0; i < device_count; i++) {
            pfds[i].fd = device_fds[i];
            pfds[i].events = POLLIN;
        }

        while (!state->stop) {
            int ret = poll(pfds, device_count, 50);
            if (ret < 0) {
                if (errno == EINTR) continue;
                break;
            }

            for (int i = 0; i < device_count; i++) {
                if (!(pfds[i].revents & POLLIN)) continue;

                struct input_event ev;
                while (1) {
                    int rc = libevdev_next_event(devices[i],
                        LIBEVDEV_READ_FLAG_NORMAL, &ev);
                    if (rc == -EAGAIN) break;
                    if (rc == LIBEVDEV_READ_STATUS_SYNC) continue;
                    if (rc < 0) goto device_error;

                    if (ev.type == EV_KEY) {
                        int modbit = code_to_mod(ev.code);
                        if (modbit) {
                            if (ev.value)
                                state->mods |= modbit;
                            else
                                state->mods &= ~modbit;
                        } else if (ev.value == 1) {
                            int cmd = keybinds_lookup(&state->kb, ev.code, state->mods);
                            if (cmd) {
                                fprintf(stderr, "dbd-timer: key %d mods %d -> cmd %d\n", ev.code, state->mods, cmd);
                                send_cmd(state->wakeup_fd, cmd);
                            }
                        }
                    }
                }
            }
        }
        device_error: ;

        for (int i = 0; i < device_count; i++) {
            libevdev_free(devices[i]);
            close(device_fds[i]);
        }
    }

    return NULL;
}

// SDL2 gamecontroller thread. Opens first available controller on start
// and listens for SDL_CONTROLLERDEVICEADDED/SDL_CONTROLLERDEVICEREMOVED for hotplug.
static void *sdl_thread(void *arg)
{
    struct sdl_state *state = arg;

    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK) < 0) {
        fprintf(stderr, "dbd-timer: SDL init: %s\n", SDL_GetError());
        return NULL;
    }

    fprintf(stderr, "dbd-timer: SDL initialized\n");

    SDL_GameController *controller = NULL;

    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            controller = SDL_GameControllerOpen(i);
            if (controller) {
                fprintf(stderr, "dbd-timer: controller %d connected\n", i);
                break;
            }
        }
    }

    while (!state->stop) {
        if (!controller) {
            SDL_Delay(16);
            continue;
        }

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_CONTROLLERDEVICEADDED:
                if (!controller) {
                    controller = SDL_GameControllerOpen(ev.cdevice.which);
                    if (controller) {
                        fprintf(stderr, "dbd-timer: controller %d connected\n", ev.cdevice.which);
                    }
                }
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                if (controller && ev.cdevice.which == SDL_JoystickInstanceID(
                        SDL_GameControllerGetJoystick(controller))) {
                    fprintf(stderr, "dbd-timer: controller disconnected\n");
                    SDL_GameControllerClose(controller);
                    controller = NULL;
                }
                break;
            case SDL_CONTROLLERBUTTONDOWN: {
                int cmd = CMD_NONE;
                switch (ev.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:   cmd = CMD_SEL1;   break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:  cmd = CMD_SEL2;   break;
                case SDL_CONTROLLER_BUTTON_A:           cmd = CMD_TOGGLE; break;
                }
                if (cmd) {
                    fprintf(stderr, "dbd-timer: ctrl button %d -> cmd %d\n", ev.cbutton.button, cmd);
                    send_cmd(state->wakeup_fd, cmd);
                }
                break;
            }
            default:
                break;
            }
        }

        SDL_Delay(16);
    }

    if (controller) SDL_GameControllerClose(controller);
    SDL_Quit();
    return NULL;
}

// Start both input threads. kb is copied so the caller may free theirs.
int input_init(struct input_state *is, int wakeup_fd, const struct keybinds *kb)
{
    memset(is, 0, sizeof(*is));
    is->wakeup_fd = wakeup_fd;

    fprintf(stderr, "dbd-timer: initializing input...\n");

    is->evdev = calloc(1, sizeof(struct libevdev_state));
    is->evdev->wakeup_fd = wakeup_fd;
    is->evdev->stop = 0;
    is->evdev->kb = *kb;

    int ret = pthread_create(&is->evdev->thread, NULL, evdev_thread, is->evdev);
    if (ret != 0) {
        fprintf(stderr, "dbd-timer: evdev thread: %s\n", strerror(ret));
        free(is->evdev);
        is->evdev = NULL;
    }

    is->sdl = calloc(1, sizeof(struct sdl_state));
    is->sdl->wakeup_fd = wakeup_fd;
    is->sdl->stop = 0;

    ret = pthread_create(&is->sdl->thread, NULL, sdl_thread, is->sdl);
    if (ret != 0) {
        fprintf(stderr, "dbd-timer: SDL thread: %s\n", strerror(ret));
        free(is->sdl);
        is->sdl = NULL;
    }

    return 0;
}

// Stop both input threads and wait for them to finish.
void input_destroy(struct input_state *is)
{
    if (is->evdev) {
        is->evdev->stop = 1;
        pthread_join(is->evdev->thread, NULL);
        free(is->evdev);
    }
    if (is->sdl) {
        is->sdl->stop = 1;
        pthread_join(is->sdl->thread, NULL);
        free(is->sdl);
    }
}
