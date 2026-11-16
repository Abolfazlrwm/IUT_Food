# ---------------------------------------------------------------------------
# IUT-Food server — multi-stage build.
#
#   docker build -t iut-food-server .
#   docker run -d -p 1234:1234 -v iut-data:/data iut-food-server
#
# Stage 1 builds only the headless server against the distro Qt 6 packages;
# stage 2 ships just the binary and the Qt runtime libraries it needs, so
# the final image stays small and never carries the toolchain.
# ---------------------------------------------------------------------------

FROM ubuntu:24.04 AS build
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build \
        qt6-base-dev libqt6charts6-dev libgl1-mesa-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j"$(nproc)" --target iut-food-server

FROM ubuntu:24.04
RUN apt-get update && apt-get install -y --no-install-recommends \
        libqt6core6t64 libqt6network6t64 libqt6sql6t64 libqt6sql6-sqlite \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=build /src/build/iut-food-server /app/iut-food-server

# v2.7: the container reports its own health.  The probe runs the same
# binary in --healthcheck mode - it sends the unauthenticated ping
# command over TCP and expects a pong naming the release.  Orchestrators
# (and `docker ps`) see "healthy" without any extra tooling inside the
# image.
HEALTHCHECK --interval=30s --timeout=5s --start-period=15s --retries=3 \
    CMD ["/app/iut-food-server", "--healthcheck", "--port", "1234"]

VOLUME /data
EXPOSE 1234
ENTRYPOINT ["/app/iut-food-server"]
CMD ["--port", "1234", "--db", "/data/iut_food.db"]
