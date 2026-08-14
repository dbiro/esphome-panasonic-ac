ESPHOME_VERSION=2026.7.4
docker run -v "${PWD}":"/config" --rm ghcr.io/esphome/esphome:${ESPHOME_VERSION?} compile ac.example.yaml