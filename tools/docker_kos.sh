#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Run a command in a KallistiOS Docker image with repo + DINK_DATA mounted.
set -eu

IMAGE=${KOS_DOCKER_IMAGE:-maishuji/dc-kos-image:15.2.1-dev-08feb26-gdb-kp08feb26}
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
DATA=${DINK_DATA:-}
CMD=${*:-make dc}

if ! command -v docker >/dev/null 2>&1; then
    echo "docker_kos: docker is not on PATH" >&2
    exit 2
fi
if ! docker info >/dev/null 2>&1; then
    echo "docker_kos: daemon not reachable (unix:///var/run/docker.sock)." >&2
    echo "  On CachyOS:  sudo systemctl start docker && sudo systemctl enable docker" >&2
    echo "  Then either: sudo usermod -aG docker \"\$USER\"  && newgrp docker" >&2
    echo "           or: sudo docker ... / sudo make docker-cdi" >&2
    exit 2
fi
if [ -z "$DATA" ] || [ ! -d "$DATA" ]; then
    echo "docker_kos: set DINK_DATA to the inner dink/ tree" >&2
    exit 2
fi

echo "docker_kos: image=$IMAGE"
echo "docker_kos: data=$DATA"
echo "docker_kos: cmd=$CMD"

# shellcheck disable=SC2086
docker run --rm \
    --user "$(id -u):$(id -g)" \
    -e HOME=/tmp \
    -e DINK_DATA=/dink \
    -v "$ROOT:/src" \
    -v "$DATA:/dink:ro" \
    -w /src \
    "$IMAGE" \
    bash -lc "set -e
      if [ -f /opt/toolchains/dc/kos/environ.sh ]; then
        . /opt/toolchains/dc/kos/environ.sh
      elif [ -n \"\${KOS_BASE:-}\" ] && [ -f \"\$KOS_BASE/environ.sh\" ]; then
        . \"\$KOS_BASE/environ.sh\"
      else
        echo 'docker_kos: no environ.sh in image' >&2
        ls -la /opt/toolchains/dc 2>/dev/null || true
        env | grep -i kos || true
        exit 2
      fi
      echo KOS_BASE=\$KOS_BASE
      which kos-cc
      $CMD"
