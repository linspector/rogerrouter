#!/bin/bash
export MSYS2_ARCH="x86_64"

# Update everything
pacman --noconfirm -Suy

# Install the required packages
pacman --noconfirm -S --needed \
    base-devel \
    git \
    mingw-w64-$MSYS2_ARCH-toolchain \
	mingw-w64-$MSYS2_ARCH-meson \
	mingw-w64-$MSYS2_ARCH-gtk4 \
	mingw-w64-$MSYS2_ARCH-ghostscript \
	mingw-w64-$MSYS2_ARCH-libadwaita \
	mingw-w64-$MSYS2_ARCH-libsoup \
	mingw-w64-$MSYS2_ARCH-speex \
	mingw-w64-$MSYS2_ARCH-spandsp \
	mingw-w64-$MSYS2_ARCH-libsndfile \
	mingw-w64-$MSYS2_ARCH-dlfcn \
	mingw-w64-$MSYS2_ARCH-nsis \
	mingw-w64-$MSYS2_ARCH-gst-plugins-base \
	mingw-w64-$MSYS2_ARCH-gst-plugins-bad \
	mingw-w64-$MSYS2_ARCH-gst-plugins-good \
	mingw-w64-$MSYS2_ARCH-desktop-file-utils 

git clone https://gitlab.gnome.org/GNOME/gssdp.git
cd gssdp
git checkout gssdp-1.4.0.1
meson _build -Dvapi=false -Dintrospection=false
ninja -C _build
ninja -C _build install
cd ..

git clone https://gitlab.gnome.org/GNOME/gupnp.git
cd gupnp
git checkout gupnp-1.4.3
meson _build -Dvapi=false -Dintrospection=false
ninja -C _build
ninja -C _build install
cd ..

git clone https://gitlab.com/tabos/rogerrouter.git
cd rogerrouter
meson _build
cd _build
ninja
ninja install

makensis build-aux/windows/roger.nsi
