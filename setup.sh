#!/bin/bash
# Copyright (c) 2017 Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
set -e

CNORM='\e[0m'
CGREEN='\e[92m'
CRED='\e[91m'
CBLUE='\e[96m'
SUCCESS="$CGREEN[+]$CNORM"
ERROR="$CRED[-]$CNORM"
INFO="$CBLUE[*]$CNORM"

INSTALL_KERN="4.4.0.21.22"
INSTALL_DOCKER="1.12.1-0~xenial"

# Add these 4 lines if you are using a proxy
# http_proxy=http://proxy.example.com:8080/
# https_proxy=http://proxy.example.com:8080/
# export http_proxy
# export https_proxy

info() {
  echo -e "$INFO $*"
}

error() {
  echo -e "$ERROR $*"
}

success() {
  echo -e "$SUCCESS $*"
}

if [[ $EUID -ne 0 ]]; then
  error "$0 needs root permissions"
  error "re-run as \`sudo -E $0\`"
  exit 1
fi

info "Updating and installing required packages with apt"
apt -y update
apt -y install \
  wget \
  xz-utils \
  rpm2cpio \
  cpio \
  libpixman-1-0 \
  apt-transport-https \
  ca-certificates \
  build-essential \
  nasm
success "apt update and install complete"

info "Adding Docker signing key to apt keyring"
apt-key adv --keyserver hkp://p80.pool.sks-keyservers.net:80 --recv-keys 58118E89F3A912897C070ADBF76221572C52609D
success "Docker signing key added"

info "Adding Docker repo to sources"
cat << EOF > /etc/apt/sources.list.d/docker.list
deb https://apt.dockerproject.org/repo ubuntu-xenial main
EOF

info "Updating sources"
apt -y update
info "Removing old lxc-docker"
apt -y purge \
  lxc-docker
info "Installing docker $INSTALL_DOCKER"
apt -y install \
  docker-engine=$INSTALL_DOCKER
success "Docker installed"

info "Adding Clear Containers runtime repo to sources"
cat << EOF > /etc/apt/sources.list.d/cc-oci-runtime.list
deb http://download.opensuse.org/repositories/home:/clearlinux:/preview:/clear-containers-2.0/xUbuntu_16.04/ /
EOF

info "Installing OBS key"
wget 'http://download.opensuse.org/repositories/home:clearlinux:preview:clear-containers-2.0/xUbuntu_16.04/Release.key'
apt-key add Release.key
rm Release.key
success "OBS key installed"

info "Installing Clear Containers runtime with apt"
apt -y update
apt -y install \
  cc-oci-runtime
success "Clear Containers runtime installed"

info "Modifying the runtime configuration"
DIR=$PWD
cd /usr/share/clear-containers/
rm clear-containers.img
ln -s clear-*-containers.img clear-containers.img
sed -ie \
  's!"image":.*$!"image": "/usr/share/clear-containers/clear-containers.img",!g' \
  /usr/share/defaults/cc-oci-runtime/vm.json
cd $DIR
success "Runtime configuration modified"

info "Enabling Clear Containers runtime for docker"
mkdir -p /etc/systemd/system/docker.service.d/
cat << EOF > /etc/systemd/system/docker.service.d/override.conf
[Service]
EOF
if [ ! -z "$http_proxy" ]; then
  cat << EOF >> /etc/systemd/system/docker.service.d/override.conf
Environment="HTTP_PROXY=$http_proxy"
Environment="no_proxy=$no_proxy"
EOF
fi
cat << EOF >> /etc/systemd/system/docker.service.d/override.conf
ExecStart=
ExecStart=/usr/bin/docker daemon -H fd:// \
  --add-runtime cor=/usr/bin/cc-oci-runtime --default-runtime=cor
EOF
if grep -q ^pids /proc/cgroups; then
  echo "TasksMax=infinity" >> /etc/systemd/system/docker.service.d/override.conf
fi
systemctl daemon-reload
systemctl restart docker
success "Clear Containers enabled and set to default for docker"

info "Installing kernel with aufs support"
info "Removing kernel $(uname -r)"
apt -y purge \
  linux-image-generic
info "Installing kernel $INSTALL_KERN"
apt -y install \
  linux-image-generic=$INSTALL_KERN \
  linux-image-virtual=$INSTALL_KERN
success "Kernel with aufs support installed"

cd /usr/share/clear-containers/

info "Downloading the clear containers image"
# NOTE: On July 10th 2017 due to issue #1
# Clear Linux team needed space on the server so the image was deleted
# Original URL: 'https://download.clearlinux.org/releases/10000/clear/clear-10000-containers.img.xz'
wget 'https://github.com/clearcontainers/cc-dirtycow-demo/releases/download/v1.0/clear-10000-containers.img.xz'
unxz clear-10000-containers.img.xz
rm -f clear-containers.img
ln -s clear-10000-containers.img clear-containers.img
success "Image download complete"

info "Downloading the clear containers kernel"
# NOTE: Due to issue #1 if this goes down a copy has been made available here:
# 'https://github.com/clearcontainers/cc-dirtycow-demo/releases/download/v1.0/linux-container-4.5-49.x86_64.rpm'
wget 'https://download.clearlinux.org/releases/10000/clear/x86_64/os/Packages/linux-container-4.5-49.x86_64.rpm'
rpm2cpio linux-container-4.5-49.x86_64.rpm | cpio -idmv
rm -f linux-container-4.5-49.x86_64.rpm
cp ./usr/share/clear-containers/vmlinux-4.5-49.container ./
rm -rf ./usr
rm -f vmlinux.container
ln -s vmlinux-4.5-49.container vmlinux.container
success "Kernel download complete"
ls -lAF $PWD

success "Setup finished. You may now build and run the exploit"
