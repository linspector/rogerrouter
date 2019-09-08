FROM debian:9.4
MAINTAINER Jan-Michael Brummer <jan.brummer@tabos.org>

RUN apt-get update

# Unprivileged compilation user:
# - control installed dependencies (no sudo)
# - generate files with default host uid/gid in bind mounts
# - when debugging reproducibility issues, one can assume the build didn't do crazy kernel stuff
RUN groupadd repro -g 1000
RUN useradd repro -m -s /bin/bash -u 1000 -g 1000
USER repro
RUN mkdir /home/repro/wd/
WORKDIR /home/repro/wd/

# Build Stage 1 - rebuild dependencies
USER root
RUN mkdir /opt/mxe
RUN chown repro: /opt/mxe
COPY packaged_deps-mxe.sh /usr/src/packaged_deps-mxe.sh
RUN /usr/src/packaged_deps-mxe.sh
RUN mkdir /usr/src/mxe/
COPY mxe/ /usr/src/mxe/

USER repro
RUN ls /usr/src/mxe/
COPY build-mxe.sh /usr/src/build-mxe.sh
RUN /usr/src/build-mxe.sh

# Build Stage 2
USER repro
CMD /bin/bash
