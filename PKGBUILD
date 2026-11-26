# Maintainer: fiftydinar <https://github.com/fiftydinar>
#
# Builds compiz from this repository, not from launchpad. The tree in this
# branch already contains every change we want, so there is no prepare() step
# and no patches to apply: what you see here is what gets built.
#
# source=() with the build running in $startdir is deliberate. Fetching
# "git+https://...#branch=main" would mean a pull request or a branch build
# compiles main instead of its own commit. The CI checks the repository out at
# the ref under test and runs makepkg inside it, so this builds exactly that.
#
# Upstream packaging details (depends, provides, conflicts, cmake flags) come
# from the Arch AUR compiz package, minus its patches - see the README.

pkgname=compiz
pkgver=0.9.14.2.r25
pkgrel=6
pkgdesc="Composite manager for Aiglx and Xgl, with plugins and CCSM (xfce-aerolike fork)"
url="https://github.com/fiftydinar/compiz-xfce-aerolike"
arch=('x86_64')
license=(
  'GPL-2.0-or-later'
  'LGPL-2.1-or-later'
  'MIT'
)

depends=(
  'glibmm'
  'glu'
  'gtk3'
  'libice'
  'libnotify'
  'libsm'
  'libwnck3'
  'libxslt'
  'metacity'
  'protobuf'
  'python'
  'python-cairo'
  'python-dbus'
  'python-gobject'
)
makedepends=(
  'boost'
  'cmake'
  'cython'
  'intltool'
  'ninja'
  'python-setuptools'
)
optdepends=(
  'xorg-xprop: grab various window properties for use in window matching rules'
)

provides=(
  "ccsm=${pkgver:0:6}"
  "compiz-bcop=${pkgver:0:6}"
  "compiz-core=${pkgver:0:6}"
  "compiz-plugins-extra=${pkgver:0:6}"
  "compiz-plugins-main=${pkgver:0:6}"
  "compizconfig-python=${pkgver:0:6}"
  "libcompizconfig=${pkgver:0:6}"
)
conflicts=(
  'ccsm'
  'compiz-bcop'
  'compiz-core'
  'compiz-fusion-plugins-experimental'
  'compiz-fusion-plugins-extra'
  'compiz-fusion-plugins-main'
  'compiz-gtk'
  'compizconfig-python'
  'libcompizconfig'
  'simple-ccsm'
)

source=()
sha256sums=()

build() {
  # Build inside the checkout so $startdir/build is the tree package() reads.
  cd "$startdir"

  local _cmake_options=(
    -B build
    -S .
    -G Ninja
    -DCMAKE_BUILD_TYPE=None
    -DCMAKE_INSTALL_PREFIX='/usr'
    -Wno-author

    -DBUILD_GTK=ON
    -DBUILD_METACITY=ON
    -DCOMPIZ_BUILD_TESTING=OFF
    -DCOMPIZ_BUILD_WITH_RPATH=OFF
    -DCOMPIZ_DISABLE_SCHEMAS_INSTALL=ON
    -DCOMPIZ_PACKAGING_ENABLED=ON
    -DCOMPIZ_WERROR=OFF

    # The Aero-like plugin set. Keep in sync with the active_plugins default in
    # metadata/core.xml.in, which the fork already sets to the same list.
    -DCOMPIZ_DEFAULT_PLUGINS="composite,opengl,decor,grid,move,place,regex,resize,snap,text,compiztoolbox,ring,wall,animation,blur,extrawm"
  )

  cmake "${_cmake_options[@]}"
  cmake --build build
}

package() {
  DESTDIR="$pkgdir" cmake --install "$startdir/build"

  # findcompiz_install needs FindCompiz.cmake in the CMake modules directory
  CMAKE_DIR=$(cmake --system-information | grep '^CMAKE_ROOT' | awk -F\" '{print $2}')
  install -Dm644 "$startdir/cmake/FindCompiz.cmake" -t "${pkgdir}${CMAKE_DIR}/Modules/"

  # gsettings schema files
  if ls "$startdir"/build/generated/glib-2.0/schemas/ | grep -qm1 .gschema.xml; then
    install -Dm644 "$startdir"/build/generated/glib-2.0/schemas/*.gschema.xml \
      -t "$pkgdir/usr/share/glib-2.0/schemas/"
  fi

  # licenses
  local i
  for i in COPYING COPYING.GPL COPYING.LGPL COPYING.MIT; do
    install -Dm644 "$startdir/$i" "$pkgdir/usr/share/licenses/$pkgname/LICENSE${i#COPYING}"
  done
}
