# andersen-mqtt

## Build the container

This repo includes a multi-arch build script powered by Docker Buildx.

### Local builds (two tags)

Build and load per-arch images into your local Docker daemon:

```bash
./scripts/build-images.sh
```

This creates:
- `opsnlops/andersen-mqtt:latest-amd64`
- `opsnlops/andersen-mqtt:latest-arm64`

### Multi-arch push (single tag)

Build and push a true multi-platform image manifest:

```bash
PUSH=true ./scripts/build-images.sh
```

This publishes a single tag `opsnlops/andersen-mqtt:latest` with both
`linux/amd64` and `linux/arm64`.

### Customize the build

You can override these environment variables:
- `IMAGE_NAME` (default: `opsnlops/andersen-mqtt`)
- `TAG` (default: `latest`)
- `PLATFORMS` (default: `linux/amd64,linux/arm64`)
- `PUSH` (default: `false`)
- `BUILDER` (default: `andersen-mqtt-builder`)

Example:

```bash
IMAGE_NAME=opsnlops/andersen-mqtt TAG=dev PLATFORMS=linux/arm64 ./scripts/build-images.sh
```

## Run

```bash
docker run -d --rm \
  --name=andersen-mqtt \
  opsnlops/andersen-mqtt:latest
```
