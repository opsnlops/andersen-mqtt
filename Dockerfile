# syntax=docker/dockerfile:1.6
#
# Builds the andersen-mqtt Debian package on Debian (never on Ubuntu), then
# installs and checks it in a clean Debian container. make_packages.sh runs this
# both locally and in GitHub Actions, so there is only one recipe.

FROM debian:trixie AS build
ARG DEBIAN_FRONTEND=noninteractive

# locales-all is here because help2man runs the fresh binary, which sets the
# en_US.UTF-8 locale at startup.
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        clang \
        cmake \
        dpkg-dev \
        file \
        git \
        help2man \
        libboost-log-dev \
        libboost-system-dev \
        locales-all \
        make \
        pkgconf && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY CMakeLists.txt LICENSE README.md ./
COPY cmake/ cmake/
COPY docs/ docs/
COPY lib/ lib/
COPY packaging/ packaging/
COPY src/ src/

RUN cmake -S /build -B /build/build -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ && \
    cmake --build /build/build --parallel

RUN mkdir /package && cd /build/build && cpack -G DEB && cp *.deb /package/


# Install the package in a clean Debian and check it before exporting it.
FROM debian:trixie-slim AS package-test
COPY --from=build /package/ /tmp/package/
COPY packaging/check-package.sh /tmp/check-package.sh
COPY packaging/check-connections.sh /tmp/check-connections.sh
RUN /tmp/check-package.sh /tmp/package/andersen-mqtt_*.deb && touch /tmp/package-validated


FROM scratch AS package
COPY --from=build /package/ /
COPY --from=package-test /tmp/package-validated /.validated
