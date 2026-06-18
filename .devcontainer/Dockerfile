FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    curl \
    g++-riscv64-linux-gnu \
    texlive-latex-extra \
    texlive-lang-spanish \
    texlive-publishers \
    latexmk \
    && rm -rf /var/lib/apt/lists/*

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
