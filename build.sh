source ./.esphomeversion
docker run --rm -v "${PWD}":"/config" "ghcr.io/esphome/esphome:${ESPHOME_VERSION?}" compile ac.example.yaml