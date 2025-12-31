#!/usr/bin/env bash
set -euo pipefail

IMAGE_NAME="${IMAGE_NAME:-opsnlops/andersen-mqtt}"
TAG="${TAG:-latest}"
PLATFORMS="${PLATFORMS:-linux/amd64,linux/arm64}"
PUSH="${PUSH:-false}"
BUILDER="${BUILDER:-andersen-mqtt-builder}"

if ! docker buildx inspect "$BUILDER" >/dev/null 2>&1; then
  docker buildx create --name "$BUILDER" --use >/dev/null
else
  docker buildx use "$BUILDER" >/dev/null
fi

if [[ "$PUSH" == "true" ]]; then
  docker buildx build \
    --platform "$PLATFORMS" \
    -t "$IMAGE_NAME:$TAG" \
    --push \
    .
  exit 0
fi

IFS=',' read -r -a platform_list <<< "$PLATFORMS"
for platform in "${platform_list[@]}"; do
  arch="${platform##*/}"
  docker buildx build \
    --platform "$platform" \
    -t "$IMAGE_NAME:$TAG-$arch" \
    --load \
    .
done

echo "Built images:"
for platform in "${platform_list[@]}"; do
  arch="${platform##*/}"
  echo " - $IMAGE_NAME:$TAG-$arch"
done
