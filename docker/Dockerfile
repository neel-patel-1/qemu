# Dockerfile for Devteroflex QEMU.
FROM ubuntu:20.04

# 1. install necessary library
RUN apt update && \
    DEBIAN_FRONTEND=noninteractive apt install git build-essential ninja-build \
    libglib2.0-dev libprotobuf-c-dev python3 pkg-config \
    libpixman-1-dev gnutls-bin -y

# 2. clone repo, build binary, install to root, and remove source code.
RUN git clone https://github.com/UlisesLuzius/qemu.git && \
    cd qemu && \
    git checkout armflex-6.0-dev && \
    git submodule update --init --recursive && \
    ./configure --target-list=aarch64-softmmu --enable-devteroflex && \
    cd build && ninja -j4 && ninja install && \
    cd ../.. && rm -rf qemu


