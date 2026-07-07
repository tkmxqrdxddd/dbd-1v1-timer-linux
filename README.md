# DBD Timer

Overlay stopwatch with two independent timers for Dead by Daylight. Renders as a transparent Wayland overlay above fullscreen games with keyboard hotkeys and Xbox controller support.

## How it works

`dbd-timer` opens a small overlay window using `wlr-layer-shell` at the `OVERLAY` layer — the highest possible layer, rendering above fullscreen games. The window is transparent with a 50% dark background, showing two timer lines with the active one highlighted in green. A companion `xdg-toplevel` surface keeps it visible as a taskbar entry.

Input comes from two sources in parallel:
- **evdev** — reads `/dev/input/event*` devices (keyboards, mice with bound buttons) filtered by configured key codes
- **SDL2** — detects Xbox/Gamepad controllers and maps D-pad and A button

## Features

- Two independent count-up timers, selectable with F1/F2 or D-pad
- Active timer highlighted in green, inactive greyed out
- Green dot indicator next to the running timer
- Smart time format: `ss:ms` under 1 minute, `mm:ss:ms` at 1 minute+
- Configurable key bindings via config file
- Controller hotplug support

## Timer behavior

Each press of the toggle key (`F` / A button / MB4):

| Condition | Action |
|-----------|--------|
| Timer **running** | Stops the timer |
| Timer **stopped** with accumulated time | Resets the selected timer to zero |
| Timer **stopped** at zero | Starts the timer |

Selecting a timer while it's running automatically stops it first.

## Key Bindings

### Keyboard / Mouse

| Key | Action |
|-----|--------|
| `F1` | Select timer 1 |
| `F2` | Select timer 2 |
| `F` | Toggle / reset / start selected timer |
| `MB4` (mouse back button) | Toggle / reset / start selected timer |
| `ESC` | Quit |

### Xbox / Gamepad

| Button | Action |
|--------|--------|
| D-pad Left | Select timer 1 |
| D-pad Right | Select timer 2 |
| A | Toggle / reset / start selected timer |

All bindings are user-configurable (see Configuration section).

## Configuration

Create `~/.config/dbd-timer/config`:

```ini
select1 = KEY_F1
select2 = KEY_F2
toggle = KEY_F, BTN_SIDE
quit = KEY_ESC
```

Key names use linux input codes (see `linux/input-event-codes.h`). Multiple keys per action are comma-separated. Lines starting with `#` are comments.

A system example is installed at `/usr/share/dbd-timer/dbd-timer.conf`.

## Requirements

- **Compositor**: Wayland compositor with `wlr-layer-shell-unstable-v1` (Sway, Hyprland, River, Wayfire, niri)
- **Permissions**: User must be in the `input` group for keyboard/mouse hotkeys
  ```sh
  sudo usermod -aG input $USER
  # re-login or reboot after
  ```
- **Dependencies** (runtime): `cairo`, `pango`, `pangocairo`, `libevdev`, `sdl2`, `wayland`
- **Dependencies** (build): `meson`, `wayland-scanner`

## Build

```sh
meson setup build
meson compile -C build
./build/dbd-timer
```

Run from a terminal to see stderr logging (detected devices, key presses, timer state changes).

## Install

### Arch Linux (AUR)

```sh
yay -S dbd-timer
# or
paru -S dbd-timer
```

### Manual (any distro)

```sh
meson setup build
meson compile -C build
sudo meson install -C build
```

Installs to `/usr/bin/dbd-timer`, desktop file to `/usr/share/applications/`, icon to `/usr/share/icons/hicolor/scalable/apps/`, and example config to `/usr/share/dbd-timer/`.

## Troubleshooting

| Problem | Cause / Fix |
|---------|-------------|
| `permission denied on /dev/input/event*` | User not in `input` group. Run `sudo usermod -aG input $USER` and re-login. |
| `no input device with bound keys found` | No keyboard or mouse detected. Check `ls /dev/input/` and verify the device has the keys configured in your bindings. |
| Overlay not visible | Compositor may not support `wlr-layer-shell` (only Sway/Hyprland/River and derivatives). On GNOME/KDE, the overlay won't render. |
| Controller not detected | Only Xbox/Gamepad controllers supported via SDL2. Try pressing a button after launch — `SDL_CONTROLLERDEVICEADDED` events are handled on connection. |

## License

MIT
