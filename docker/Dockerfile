# Dockerfile for Devteroflex QEMU.
FROM ubuntu:20.04

# 1. install necessary library
RUN apt update && \
    DEBIAN_FRONTEND=noninteractive apt install git build-essential ninja-build vim gdb \
    libglib2.0-dev libprotobuf-c-dev python3 pkg-config \
    libpixman-1-dev gnutls-bin -y

# 2. clone the Hardware repo
RUN git clone https://github.com/parsa-epfl/KnockoutKraken.git && \
    cd KnockoutKraken && \
    git checkout for_qemu_build && \
    cd ..

# 3. clone the AWS FPGA repo
RUN git clone https://github.com/aws/aws-fpga.git && \
    cd aws-fpga/sdk/userspace/ && \
    ./mkall_fpga_mgmt_tools.sh && \
    cd ../../../

# 4. clone repo, build binary, install to root, and remove source code.
RUN git clone https://github.com/UlisesLuzius/qemu.git && \
    cd qemu && \
    git checkout armflex-6.2-dev && \
    git submodule update --init --recursive && \
    ./configure --target-list=aarch64-softmmu \ 
    --enable-devteroflex=$(pwd)/../KnockoutKraken/regression/src/client \
    --enable-aws-fpga=$(pwd)/../aws-fpga/sdk/userspace/ && \
    cd build && ninja && ninja install


