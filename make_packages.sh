#!/usr/bin/env bash
# Build and check the Debian package for this machine's architecture, inside
# Debian. GitHub Actions runs this same script on amd64 and arm64 runners.
# The package lands in package/.

set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
export_dir=$(mktemp -d "${TMPDIR:-/tmp}/andersen-mqtt-package.XXXXXX")
trap 'rm -rf "$export_dir"' EXIT

docker build --pull --target package --output "type=local,dest=$export_dir" "$script_dir"

if [ ! -f "$export_dir/.validated" ]; then
    echo "The package was not validated" >&2
    exit 1
fi

shopt -s nullglob
packages=("$export_dir"/andersen-mqtt_*.deb)
if [ "${#packages[@]}" -ne 1 ]; then
    echo "Expected exactly one andersen-mqtt package, found ${#packages[@]}" >&2
    exit 1
fi

mkdir -p "$script_dir/package"
rm -f "$script_dir"/package/andersen-mqtt_*.deb
cp "${packages[@]}" "$script_dir/package/"
ls -lh "$script_dir"/package/*.deb
