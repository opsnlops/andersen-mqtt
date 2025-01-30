FROM debian:bookworm AS build

RUN apt update && apt upgrade -y && apt install -y \
    clang \
    cmake \
    git \
    libboost1.81-all-dev \
    pkg-config && \
    rm -rf /var/lib/apt/*

WORKDIR /build
COPY src/ src/
COPY lib/ lib/
COPY CMakeLists.txt ./

WORKDIR /build/build
RUN cmake .. && make



FROM debian:bookworm-slim AS runtime

# Install what we need
RUN apt update && apt upgrade -y && apt install -y \
    libboost-atomic1.81.0 \
    libboost-chrono1.81.0 \
    libboost-log1.81.0 \
    libboost-regex1.81.0 \
    libboost-system1.81.0 \
    locales-all && \
    rm -rf /var/lib/apt/*

WORKDIR /app
COPY --from=build /build/build/andersen_mqtt /app/andersen_mqtt

CMD ["/app/andersen_mqtt"]
