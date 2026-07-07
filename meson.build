project('dbd-timer', 'c',
  version: '1.0.0',
  license: 'MIT',
  meson_version: '>=0.49.0')

wl_scanner = find_program('wayland-scanner')
wl_prot_dir = 'protocols'

layer_xml = wl_prot_dir / 'wlr-layer-shell-unstable-v1.xml'
xdg_xml = wl_prot_dir / 'xdg-shell.xml'

layer_c = custom_target('layer-c',
  input: layer_xml, output: 'layer-protocol.c',
  command: [wl_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
  build_by_default: true)

layer_h = custom_target('layer-h',
  input: layer_xml, output: 'layer-protocol.h',
  command: [wl_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
  build_by_default: true)

xdg_c = custom_target('xdg-c',
  input: xdg_xml, output: 'xdg-protocol.c',
  command: [wl_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
  build_by_default: true)

xdg_h = custom_target('xdg-h',
  input: xdg_xml, output: 'xdg-protocol.h',
  command: [wl_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
  build_by_default: true)

layer_dep = declare_dependency(sources: [layer_c, layer_h])
xdg_dep = declare_dependency(sources: [xdg_c, xdg_h])

deps = [
  dependency('wayland-client'),
  dependency('cairo'),
  dependency('pango'),
  dependency('pangocairo'),
  dependency('sdl2'),
  dependency('libevdev'),
  layer_dep,
  xdg_dep,
]

executable('dbd-timer',
  'src/main.c', 'src/wayland.c', 'src/render.c', 'src/timer.c', 'src/input.c', 'src/config.c',
  dependencies: deps,
  install: true,
  install_dir: '/usr/bin')

install_data('dbd-timer.desktop',
  install_dir: '/usr/share/applications')

install_data('dbd-timer.svg',
  install_dir: '/usr/share/icons/hicolor/scalable/apps')

install_data('dbd-timer.conf',
  install_dir: '/usr/share/dbd-timer')
