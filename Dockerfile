# Multi-stage build for C++ services
FROM ubuntu:22.04 AS base

# Install build dependencies including OpenSSL for NATS
RUN apt-get update && apt-get install -y \
    cmake \
    ninja-build \
    g++ \
    pkg-config \
    protobuf-compiler \
    libprotobuf-dev \
    libyaml-cpp-dev \
    libspdlog-dev \
    libfmt-dev \
    libssl-dev \
    zlib1g-dev \
    wget \
    build-essential \
    libgtest-dev \
    && rm -rf /var/lib/apt/lists/*

# Install Catch2 from source
RUN cd /tmp && \
    wget https://github.com/catchorg/Catch2/archive/refs/tags/v3.4.0.tar.gz && \
    tar -xzf v3.4.0.tar.gz && \
    cd Catch2-3.4.0 && \
    mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF && \
    make -j$(nproc) && \
    make install && \
    ldconfig && \
    cd / && rm -rf /tmp/Catch2-3.4.0*

# Download and build NATS C client from source (with OpenSSL)
RUN cd /tmp && \
    wget https://github.com/nats-io/nats.c/archive/refs/tags/v3.7.0.tar.gz && \
    tar -xzf v3.7.0.tar.gz && \
    cd nats.c-3.7.0 && \
    mkdir build && cd build && \
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DNATS_BUILD_EXAMPLES=OFF \
        -DNATS_BUILD_STREAMING=OFF && \
    make -j$(nproc) && \
    make install && \
    ldconfig && \
    cd / && rm -rf /tmp/nats.c-3.7.0*

WORKDIR /app

# Copy source code
COPY libs/ libs/
COPY proto/ proto/
COPY services/ services/
COPY tests/ tests/
COPY CMakeLists.txt .
COPY config.yaml .

# Generate protobuf files directly in current directory
RUN cd proto && \
    protoc -I=. --cpp_out=.. messages.proto && \
    echo "✅ Protobuf files generated successfully"

# Build stage
FROM base AS builder

# Clean any existing build artifacts and create fresh build directory
RUN rm -rf build
RUN mkdir build && cd build && \
    cmake -GNinja .. && \
    ninja

# Runtime stage for portfolio_manager  
FROM ubuntu:22.04 AS portfolio_manager

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libstdc++6 \
    libprotobuf-dev \
    libyaml-cpp-dev \
    libspdlog-dev \
    libfmt-dev \
    libssl-dev \
    zlib1g-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Copy NATS shared libraries from builder stage
COPY --from=builder /usr/local/lib/libnats* /usr/local/lib/
COPY --from=builder /usr/local/include/nats /usr/local/include/nats/
RUN ldconfig

WORKDIR /app
COPY --from=builder /app/build/services/portfolio_manager/portfolio_manager .
COPY --from=builder /app/build/libproto_files.a .
COPY --from=builder /app/config.yaml .

EXPOSE 8080
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD ps aux | grep portfolio_manager | grep -v grep || exit 1

CMD ["./portfolio_manager", "config.yaml"]
