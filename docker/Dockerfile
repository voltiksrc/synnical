FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    libcurl4-openssl-dev \
    libarchive-dev \
    libtomlplusplus-dev \
    libsqlite3-dev \
    libsolv-dev \
    libsolvext-dev \
    libsodium-dev \
    libssl-dev \
    ca-certificates \
    curl \
    git \
    python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /synnical

CMD ["/bin/bash"]
