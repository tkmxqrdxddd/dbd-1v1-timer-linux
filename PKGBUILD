# Maintainer: Your Name <you@example.com>

pkgname=dbd-timer
pkgver=1.0.0
pkgrel=1
pkgdesc="Overlay stopwatch with two independent timers, Wayland overlay, and gamepad support"
arch=('x86_64')
url="https://github.com/example/dbd-timer"
license=('MIT')
depends=(
  'cairo'
  'pango'
  'pangocairo'
  'libevdev'
  'sdl2'
  'wayland'
)
makedepends=('meson' 'wayland-scanner')
source=("$pkgname-$pkgver.tar.gz::https://github.com/example/dbd-timer/archive/v$pkgver.tar.gz")
sha256sums=('SKIP')

build() {
  arch-meson "$pkgname-$pkgver" build
  meson compile -C build
}

check() {
  true
}

package() {
  DESTDIR="$pkgdir" meson install -C build
}
