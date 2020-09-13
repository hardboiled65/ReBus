# Maintainer: Yujeonja <hardboiled65@gmail.com>
pkgname=rebus
pkgver=0.1.0
pkgrel=1
epoch=
pkgdesc="ReBus is an IPC stack for Laniakea desktop. Inspired by REST API architecture."
arch=('x86_64')
url="https://github.com/hardboiled65/ReBus"
license=('MIT')
groups=()
depends=('qt5-base>=5.15.0')
makedepends=()
checkdepends=()
optdepends=()
provides=()
conflicts=()
replaces=()
backup=()
options=()
install=
changelog=
source=("$pkgname-$pkgver.tar.gz"
        "$pkgname-$pkgver.patch")
noextract=()
md5sums=()
validpgpkeys=()

prepare() {
	cd "$pkgname-$pkgver"
	patch -p1 -i "$srcdir/$pkgname-$pkgver.patch"
}

build() {
	cd "$pkgname-$pkgver"
	./configure --prefix=/usr
	make
}

check() {
	cd "$pkgname-$pkgver"
	make -k check
}

package() {
	cd "$pkgname-$pkgver"
	make DESTDIR="$pkgdir/" install
}
