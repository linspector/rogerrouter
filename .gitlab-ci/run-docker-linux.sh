#!/bin/bash

set -e

TAG="registry.gitlab.com/tabos/rogerrouter/linux:v5"

cd "$(dirname "$0")"
docker build --tag "${TAG}" --file "Dockerfile.linux" .

if [ "$1" = "--push" ]; then
  docker login registry.gitlab.com
  docker push $TAG
else
  docker run --rm --security-opt label=disable \
    --volume "$(pwd)/..:/home/user/app" --workdir "/home/user/app" \
    --tty --interactive "${TAG}" bash
fi
