FROM debian:trixie-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    pkg-config \
    libboost-all-dev \
    libhiredis-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY CMakeLists.txt .
COPY include include
COPY src src
COPY tests tests

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build -j2 && \
    ctest --test-dir build --output-on-failure

FROM debian:trixie-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    libhiredis-dev \
    wget \
    && rm -rf /var/lib/apt/lists/*

RUN useradd --system --uid 10002 --create-home app
WORKDIR /app
COPY --from=builder /src/build/session-service /app/session-service
USER app
EXPOSE 8081
CMD ["/app/session-service"]
