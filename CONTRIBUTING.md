# Contributing

## Development setup

```sh
meson setup build && meson compile -C build
```

## Code style

- No comments unless unavoidable
- No trailing whitespace
- Follow existing patterns (same libraries, same naming conventions)

## Wayland protocols

Protocol XML is bundled in `protocols/`. Add new ones to `meson.build` with matching `custom_target` entries.

## Testing

Manual: run from a terminal and watch stderr for input events.

## Pull requests

Open a PR against `master`.
