FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    curl \
    xz-utils \
    qemu-system-misc \
    texlive-latex-extra \
    texlive-lang-spanish \
    texlive-publishers \
    latexmk \
    && rm -rf /var/lib/apt/lists/*

# Bare-metal RISC-V toolchain (riscv-none-elf-gcc/g++ + newlib) from xPack.
# Ubuntu ships no g++ for the *-unknown-elf (bare-metal) target, so vendor a prebuilt one.
ENV RISCV_GCC_VERSION=14.2.0-3
ENV RISCV_TOOLCHAIN_PREFIX=riscv-none-elf-
ENV PATH=/opt/riscv-none-elf-gcc/bin:${PATH}
RUN curl -L https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases/download/v${RISCV_GCC_VERSION}/xpack-riscv-none-elf-gcc-${RISCV_GCC_VERSION}-linux-x64.tar.gz \
        | tar xz -C /tmp \
    && mv /tmp/xpack-riscv-none-elf-gcc-${RISCV_GCC_VERSION} /opt/riscv-none-elf-gcc

# Pre-compile SystemC 2.3.4 so project builds skip the download step
ENV SYSTEMC_HOME=/opt/systemc
RUN curl -L https://github.com/accellera-official/systemc/archive/refs/tags/2.3.4.tar.gz \
        | tar xz -C /tmp \
    && cmake -S /tmp/systemc-2.3.4 -B /tmp/systemc-build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=17 \
        -DCMAKE_INSTALL_PREFIX=${SYSTEMC_HOME} \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DBUILD_SOURCE_DOCUMENTATION=OFF \
    && cmake --build /tmp/systemc-build -j$(nproc) \
    && cmake --install /tmp/systemc-build \
    && rm -rf /tmp/systemc-2.3.4 /tmp/systemc-build
