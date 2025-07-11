FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    gdb \
    clang \
    git \
    wget \
    curl \
    unzip \
    pkg-config \
    libssl-dev \
    libprotobuf-dev \
    protobuf-compiler \
    libnats-dev \
    catch2 \
    python3 \
    python3-pip \
    python3-venv \
    && apt-get clean

WORKDIR /app

# Copy the entire project
COPY . .

# Build the project directly without using the script
RUN mkdir -p build && \
    cd build && \
    cmake .. -GNinja && \
    ninja

CMD ["./build/services/portfolio_manager/portfolio_manager"]
