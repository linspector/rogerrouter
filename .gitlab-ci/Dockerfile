FROM fedora:30

RUN dnf update -y \
    && dnf install -y 'dnf-command(builddep)' \
    && dnf install -y meson \
                      gcc \
                      gettext \
                      libgs-devel \
                      glib2-devel \
                      gdk-pixbuf2-devel \
                      libsoup-devel \
                      speex-devel \
                      spandsp-devel \
                      json-glib-devel \
                      libsndfile-devel \
                      gupnp-devel \
                      isdn4k-utils-devel \
                      gtk3-devel \
                      poppler-glib-devel \
    && dnf clean all

ARG HOST_USER_ID=5555
ENV HOST_USER_ID ${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -ms /bin/bash user

USER user
WORKDIR /home/user

ENV LANG C.UTF-8
