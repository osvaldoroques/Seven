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

RUN pip install --upgrade pip

# Optional: Poetry if you prefer Python dependency management
RUN pip install poetry

# Set up user for VS Code
RUN useradd -m vscode && echo "vscode ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

USER vscode
WORKDIR /workspaces/robo-advisor
