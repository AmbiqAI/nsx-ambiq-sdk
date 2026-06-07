FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
ARG UV_VERSION=0.7.12
ARG ARM_GNU_VERSION=14.3.rel1
ARG ATFE_VERSION=22.1.0
ARG ATFE_RELEASE_TAG=release-22.1.0-ATfE
ARG ARM_COMPILER_VERSION=6.24
ARG ARM_COMPILER_REL=19

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        cmake \
        curl \
        g++ \
        gcc \
        git \
        libncurses6 \
        libtinfo6 \
        make \
        ninja-build \
        python3 \
        python3-venv \
        xz-utils \
    && rm -rf /var/lib/apt/lists/*

RUN curl -LsSf https://astral.sh/uv/${UV_VERSION}/install.sh | UV_INSTALL_DIR=/usr/local/bin sh

ENV UV_CACHE_DIR=/tmp/uv-cache
ENV GCC_ROOT=/opt/toolchains/gcc
ENV ACFE_ROOT=/opt/toolchains/acfe
ENV ATFE_ROOT=/opt/toolchains/atfe

RUN set -eux; \
    host_arch="$(uname -m)"; \
    case "${host_arch}" in \
        x86_64) gcc_arch="x86_64"; gcc_md5="17272b6c72d476c82b692a06ada0636c" ;; \
        aarch64) gcc_arch="aarch64"; gcc_md5="5b44bdd1d983247ec153fe548b4ff8ed" ;; \
        *) echo "Unsupported Arm GNU host architecture: ${host_arch}" >&2; exit 1 ;; \
    esac; \
    gcc_archive="arm-gnu-toolchain-${ARM_GNU_VERSION}-${gcc_arch}-arm-none-eabi.tar.xz"; \
    gcc_url="https://developer.arm.com/-/media/Files/downloads/gnu/${ARM_GNU_VERSION}/binrel/${gcc_archive}"; \
    mkdir -p /tmp/gcc; \
    curl -fsSL "${gcc_url}" -o "/tmp/gcc/${gcc_archive}"; \
    echo "${gcc_md5}  /tmp/gcc/${gcc_archive}" | md5sum -c -; \
    mkdir -p "${GCC_ROOT}"; \
    tar -xJf "/tmp/gcc/${gcc_archive}" -C "${GCC_ROOT}" --strip-components=1; \
    test -x "${GCC_ROOT}/bin/arm-none-eabi-gcc"; \
    test -x "${GCC_ROOT}/bin/arm-none-eabi-ar"; \
    rm -rf /tmp/gcc

RUN set -eux; \
    host_arch="$(uname -m)"; \
    case "${host_arch}" in \
        x86_64) atfe_arch="x86_64" ;; \
        aarch64) atfe_arch="AArch64" ;; \
        *) echo "Unsupported ATfE host architecture: ${host_arch}" >&2; exit 1 ;; \
    esac; \
    atfe_archive="ATfE-${ATFE_VERSION}-Linux-${atfe_arch}.tar.xz"; \
    atfe_url="https://github.com/arm/arm-toolchain/releases/download/${ATFE_RELEASE_TAG}/${atfe_archive}"; \
    mkdir -p /tmp/atfe; \
    curl -fsSL "${atfe_url}" -o "/tmp/atfe/${atfe_archive}"; \
    curl -fsSL "${atfe_url}.sha256" -o "/tmp/atfe/${atfe_archive}.sha256"; \
    cd /tmp/atfe; \
    sha256sum -c "${atfe_archive}.sha256"; \
    mkdir extract; \
    tar -xJf "${atfe_archive}" -C extract; \
    extracted="$(find extract -mindepth 1 -maxdepth 1 -type d | head -n 1)"; \
    mkdir -p "${ATFE_ROOT}"; \
    cp -a "${extracted}/." "${ATFE_ROOT}/"; \
    test -x "${ATFE_ROOT}/bin/clang"; \
    test -x "${ATFE_ROOT}/bin/llvm-ar"; \
    rm -rf /tmp/atfe

RUN set -eux; \
    host_arch="$(uname -m)"; \
    case "${host_arch}" in \
        x86_64) acfe_archive="standalone-linux-x86_64-rel.tar.gz" ;; \
        aarch64) acfe_archive="standalone-linux-armv8l_64-rel.tar.gz" ;; \
        *) echo "Unsupported ACfE host architecture: ${host_arch}" >&2; exit 1 ;; \
    esac; \
    acfe_url="https://artifacts.tools.arm.com/arm-compiler/${ARM_COMPILER_VERSION}/${ARM_COMPILER_REL}/${acfe_archive}"; \
    mkdir -p /tmp/acfe; \
    curl -fsSL "${acfe_url}" -o "/tmp/acfe/${acfe_archive}"; \
    mkdir /tmp/acfe/extract; \
    tar -xzf "/tmp/acfe/${acfe_archive}" -C /tmp/acfe/extract; \
    mkdir -p "${ACFE_ROOT}"; \
    cp -a /tmp/acfe/extract/. "${ACFE_ROOT}/"; \
    test -x "${ACFE_ROOT}/bin/armclang"; \
    test -x "${ACFE_ROOT}/bin/armar"; \
    test -x "${ACFE_ROOT}/bin/armlink"; \
    test -x "${ACFE_ROOT}/bin/fromelf"; \
    rm -rf /tmp/acfe

ENV UV_LINK_MODE=copy

WORKDIR /workspace
