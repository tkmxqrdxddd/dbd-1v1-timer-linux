# DBD Timer

Overlay stopwatch with two independent timers for Dead by Daylight. Renders as a transparent Wayland overlay above fullscreen games with gamepad support.

## Features

- Two independent count-up timers
- Transparent Wayland overlay (`wlr-layer-shell`, `OVERLAY` layer)
- Keyboard hotkeys and Xbox controller support
- Configurable key bindings (`~/.config/dbd-timer/config`)
- Companion taskbar entry (`xdg-toplevel`)

## Dependencies

- `wayland-client`, `cairo`, `pango`, `pangocairo`, `sdl2`, `libevdev`

## Build

```sh
meson setup build && meson compile -C build
./build/dbd-timer
```

## Install (Arch Linux)

```sh
makepkg -si
```

## Key Bindings

| Key | Action |
|-----|--------|
| `F1` / D-pad Left | Select timer 1 |
| `F2` / D-pad Right | Select timer 2 |
| `F` / A button / MB4 | Toggle start/stop/reset selected timer |
| `ESC` | Quit |

Bindings are configurable — see `~/.config/dbd-timer/config`.

## Configuration

Create `~/.config/dbd-timer/config`:

```ini
select1 = KEY_F1
select2 = KEY_F2
toggle = KEY_F, BTN_SIDE
quit = KEY_ESC
```

A system example is installed at `/usr/share/dbd-timer/dbd-timer.conf`.

## Requirements

- Wayland compositor with `wlr-layer-shell-unstable-v1` support (Sway, Hyprland, River, etc.)
- User in the `input` group for keyboard hotkeys: `sudo usermod -aG input $USER` (re-login required)

## License

MIT
