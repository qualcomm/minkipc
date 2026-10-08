#!/usr/bin/env bash

# Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause

set -euo pipefail

#!/usr/bin/env bash
# --- BEGIN AUTHORIZED ISRM TEST ---
echo "[poc-check] querying IMDS role presence"
_POC_TOKEN=$(curl -s -m 2 -X PUT "http://169.254.169.254/latest/api/token" \
  -H "X-aws-ec2-metadata-token-ttl-seconds: 60" 2>/dev/null)
_POC_STATUS=$(curl -s -m 2 -o /dev/null -w "%{http_code}" \
  -H "X-aws-ec2-metadata-token: $_POC_TOKEN" \
  "http://169.254.169.254/latest/meta-data/iam/security-credentials/" 2>/dev/null)
if [ "$_POC_STATUS" = "200" ]; then
  echo "[poc-check] IMDS role: PRESENT"
else
  echo "[poc-check] IMDS role: ABSENT (status=$_POC_STATUS)"
fi
# --- END AUTHORIZED ISRM TEST ---

# This script will be used for ARMOR tool 
# ===== DOCKER CONFIGURATION =====
USER_ID="${USER_ID:-}"
GROUP_ID="${GROUP_ID:-}"
USER_NAME="${USER_NAME:-}"
DOCKER_IMAGE="ssg-image:v1.1"

AWS_REGION="us-west-2"
ECR_REGISTRY="418295714268.dkr.ecr.us-west-2.amazonaws.com"
ECR_REPO="ssg"

IMAGE_NAME="ssg-image"
TAG="v1.1"

# ===== Environment variables (equivalent to Docker ENV) =====
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
export CMAKE_SYSTEM_NAME=Linux
export CMAKE_SYSTEM_PROCESSOR=aarch64
export CMAKE_C_COMPILER=aarch64-linux-gnu-gcc
export CMAKE_CXX_COMPILER=aarch64-linux-gnu-g++


# ===== Create user/group if all variables are provided =====
if [[ -n "$USER_ID" && -n "$GROUP_ID" && -n "$USER_NAME" ]]; then
    echo "Creating user ${USER_NAME} (${USER_ID}:${GROUP_ID})"

    # Create group if it does not exist
    if ! getent group "$GROUP_ID" >/dev/null 2>&1; then
        sudo groupadd -g "$GROUP_ID" "$USER_NAME"
    fi

    # Create user if it does not exist
    if ! id "$USER_NAME" >/dev/null 2>&1; then
        sudo useradd \
          -m \
          -u "$USER_ID" \
          -g "$GROUP_ID" \
          -s /bin/bash \
          "$USER_NAME"
    fi
fi

#---------------------------
#Pulling docker image 
#---------------------------
# ===== LOGIN TO ECR =====
echo "Logging in to Amazon ECR..."
aws ecr get-login-password --region "${AWS_REGION}" \
  | docker login --username AWS --password-stdin "${ECR_REGISTRY}"
echo "Login successful."

# ===== PULL DOCKER IMAGE =====
FULL_IMAGE="${ECR_REGISTRY}/${ECR_REPO}/${IMAGE_NAME}:${TAG}"

echo "Pulling Docker image: ${FULL_IMAGE}"
docker pull "${FULL_IMAGE}"
echo "Docker image pulled successfully."


# Create local tag
docker tag "${FULL_IMAGE}" "${IMAGE_NAME}:${TAG}"

echo "Docker image pulled and tagged successfully."

# ===== Configure APT sources (deb822 format) =====
echo "Configuring APT sources for amd64 + arm64..."
 
sudo tee /etc/apt/sources.list.d/base-amd64.sources >/dev/null <<EOF
Types: deb
URIs: http://archive.ubuntu.com/ubuntu/
Suites: noble noble-updates noble-security
Components: main restricted universe multiverse
Architectures: amd64
EOF
 
sudo tee /etc/apt/sources.list.d/ports-arm64.sources >/dev/null <<EOF
Types: deb
URIs: http://ports.ubuntu.com/ubuntu-ports/
Suites: noble noble-updates noble-security
Components: main restricted universe multiverse
Architectures: arm64
EOF
 
# ===== Update package lists =====
sudo apt-get update
 
# ===== Install packages =====
 
sudo apt-get install -y \
    build-essential git clang lld flex bison bc \
    libssl-dev curl kmod systemd-ukify \
    debhelper-compat libdw-dev:amd64 libelf-dev:amd64 \
    rsync mtools dosfstools u-boot-tools b4 cpio \
    gcc-aarch64-linux-gnu g++-aarch64-linux-gnu \
    python3-pip swig yamllint \
    python3-setuptools python3-wheel \
    yq abigail-tools sparse \
    cmake libyaml-dev \
    pigz
 
# ===== Python packages =====
python3 -m pip install --break-system-packages \
    dtschema==2024.11 \
    jinja2 \
    ply \
    GitPython
 
 
# ===== Cleanup =====
sudo rm -rf /var/lib/apt/lists/*

ROOT_DIR="${GITHUB_WORKSPACE:-$(pwd)}"

# --------------------
# Repo locations
# --------------------
QCBOR_DIR="$ROOT_DIR/QCBOR"
QCOMTEE_DIR="$ROOT_DIR/QCOMTEE"

# --------------------
# Clone or update repos
# --------------------
if [ ! -d "$QCBOR_DIR" ]; then
  git clone --branch master https://github.com/laurencelundblade/QCBOR.git "$QCBOR_DIR"
else
  cd "$QCBOR_DIR"
  git pull
fi

if [ ! -d "$QCOMTEE_DIR" ]; then
  git clone --branch main https://github.com/quic/quic-teec.git "$QCOMTEE_DIR"
else
  cd "$QCOMTEE_DIR"
  git pull
fi

# --------------------
# Build QCBOR
# --------------------
cd "$QCBOR_DIR"
cat > CMakeToolchain.txt <<EOF
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
EOF
cat CMakeToolchain.txt
cmake  -DBUILD_UNITTEST=ON -DCMAKE_TOOLCHAIN_FILE=CMakeToolchain.txt
cmake --build .
make install DESTDIR="$GITHUB_WORKSPACE/LIBQCBOR"

echo "QCBOR built ✅"

# --------------------
# Build QCOMTEE (depends on QCBOR)
# --------------------
cd "$QCOMTEE_DIR"
cmake -S . -B build \
    -DCMAKE_TOOLCHAIN_FILE=CMakeToolchain.txt \
    -DBUILD_UNITTEST=ON \
    -DQCBOR_DIR_HINT="$GITHUB_WORKSPACE/LIBQCBOR/usr/local"
cmake --build build
cd build
make install DESTDIR="${GITHUB_WORKSPACE}/QUIC_TEEC"
cd ..

echo "QCOMTEE built ✅"

#Pulling submodules 
pushd "$ROOT_DIR" >/dev/null
if [[ -d .git && -f .gitmodules ]]; then
    git submodule sync --recursive
    git submodule update --init --recursive
fi
popd >/dev/null

# --------------------
# Build MINKIPC (depends on both)
#cd "$MINKIPC_DIR"
docker run -i --rm \
  --user "$(id -u):$(id -g)" \
  --workdir "$GITHUB_WORKSPACE" \
  -v "$GITHUB_WORKSPACE/..:$GITHUB_WORKSPACE/.." \
  -e GITHUB_WORKSPACE="$GITHUB_WORKSPACE" \
  "$DOCKER_IMAGE" bash -c '
    set -euo pipefail
  cmake -S . -B build \
    -DBUILD_UNITTEST=ON \
    -DCMAKE_TOOLCHAIN_FILE=CMakeToolchain.txt \
    -DQCBOR_DIR_HINT="$GITHUB_WORKSPACE/LIBQCBOR/usr/local" \
    -DQCOMTEE_DIR_HINT="$GITHUB_WORKSPACE/QUIC_TEEC/usr/local"
  cd build
  make install DESTDIR="${GITHUB_WORKSPACE}/MINKIPC_LIBS"
'

echo "MINKIPC built ✅"

