# Multi-stage Dockerfile for SwiftAgent.
#
# Build the CLI:
#   docker build -t swiftagent:dev -f docker/Dockerfile.cli .
# Run:
#   docker run --rm -it swiftagent:dev --help
#
# Build the Python SDK wheel (used by the wheel.yml workflow):
#   docker build -t swiftagent:wheel -f docker/Dockerfile.wheel --build-arg PYTHON=3.12 .
#
# Build the dev image (full toolchain + Python + examples + tests):
#   docker build -t swiftagent:dev-full -f docker/Dockerfile.dev .

ARG BASE=debian:bookworm-slim
ARG PYTHON_BASE=python:3.12-slim-bookworm

# ---- Runtime image: minimal, only the CLI binary ----
FROM ${BASE} AS cli
ARG TARGETARCH
RUN apt-get update && apt-get install -y --no-install-recommends \
        libstdc++6 \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*
COPY build/swiftagent /usr/local/bin/swiftagent
RUN useradd -m -u 1000 swift && mkdir -p /workspace && chown -R swift:swift /workspace
USER swift
WORKDIR /workspace
ENTRYPOINT ["/usr/local/bin/swiftagent"]
CMD ["--help"]

# ---- Wheel build: pybind11 + scikit-build-core + project source ----
FROM ${PYTHON_BASE} AS wheel
ARG PYTHON
ENV PIP_NO_CACHE_DIR=1
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        git \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . /src
RUN pip install --upgrade pip \
    && pip install "pybind11>=2.12" "scikit-build-core>=0.10" "cmake>=3.24" "ninja"
RUN pip wheel . --no-deps --wheel-dir /wheels

# ---- Dev image: toolchain + Python + can run tests/examples ----
FROM ${PYTHON_BASE} AS dev
ARG PYTHON
ENV PIP_NO_CACHE_DIR=1
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        git \
        clang-format \
        lcov \
        doxygen \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . /src
RUN pip install --upgrade pip \
    && pip install "pybind11>=2.12" "scikit-build-core>=0.10" "pytest>=8" "ninja"
RUN cmake -S . -B build -G Ninja -DSWIFTAGENT_BUILD_EXAMPLES=ON && cmake --build build -j
RUN ctest --test-dir build --output-on-failure
CMD ["/bin/bash"]
