#!/bin/bash -ex
# Build MXE environment

cd /opt/
git clone https://github.com/mxe/mxe.git
cd mxe/

# Copy custom mxe packages
cp /usr/src/mxe/* src/

make capi gtk3 speex libsndfile speexdsp spandsp gst-plugins-good gst-plugins-bad librsvg libsoup json-glib gssdp gupnp ghostscript adwaita-icon-theme poppler MXE_TARGETS=i686-w64-mingw32.shared.posix
