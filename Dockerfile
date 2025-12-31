# syntax=docker/dockerfile:1.6
FROM debian:trixie AS build
ARG DEBIAN_FRONTEND=noninteractive

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    rm -f /var/cache/apt/archives/lock && \
    apt-get update && apt-get install -y --no-install-recommends \
    clang \
    ca-certificates \
    make \
    cmake \
    git \
    libboost-all-dev \
    pkg-config && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY src/ src/
COPY lib/ lib/
COPY CMakeLists.txt ./

RUN cmake -S /build -B /build/build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build /build/build --parallel



FROM debian:trixie-slim AS runtime
ARG DEBIAN_FRONTEND=noninteractive

# Install what we need
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    rm -f /var/cache/apt/archives/lock && \
    apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    libboost-atomic-dev \
    libboost-chrono-dev \
    libboost-log-dev \
    libboost-regex-dev \
    libboost-system-dev \
    locales-all && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=build /build/build/andersen_mqtt /app/andersen_mqtt

CMD ["/app/andersen_mqtt"]
