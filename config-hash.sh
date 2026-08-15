source ./.esphomeversion
docker run --rm -v "${PWD}":"/config" "ghcr.io/esphome/esphome:${ESPHOME_VERSION?}" -q config-hash ac.example.yaml
