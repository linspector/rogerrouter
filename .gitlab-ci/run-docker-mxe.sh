#!/bin/bash

set -e

TAG="registry.gitlab.com/tabos/rogerrouter/mxe:v1"

rm -rf mxe
mkdir mxe
cp ../platform/windows/mxe-pkgs/* mxe

cd "$(dirname "$0")"
docker build --tag "${TAG}" --file "Dockerfile.mxe" .
rm -rf mxe

if [ "$1" = "--push" ]; then
  docker login registry.gitlab.com
  docker push $TAG
else
  docker run --rm --security-opt label=disable \
    --volume "$(pwd)/..:/home/user/app" --workdir "/home/user/app" \
    --tty --interactive "${TAG}" bash
fi

