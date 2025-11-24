#!/usr/bin/env sh
#
##!/bin/bash
# disable_interceptors.sh

# Disable Ubuntu's apport
sudo systemctl stop apport
sudo systemctl disable apport

# Disable systemd-coredump
sudo systemctl mask systemd-coredump@.service
sudo systemctl stop systemd-coredump.socket
sudo systemctl disable systemd-coredump.socket

# Create core directory
sudo mkdir -p /tmp/cores
sudo chmod 1777 /tmp/cores

# Configure kernel parameters
echo "/tmp/cores/core.%e.%p.%t" | sudo tee /proc/sys/kernel/core_pattern
echo 2 | sudo tee /proc/sys/fs/suid_dumpable

# Make persistent
echo "kernel.core_pattern=/tmp/cores/core.%e.%p.%t" | sudo tee -a /etc/sysctl.d/99-coredump.conf
echo "fs.suid_dumpable=2" | sudo tee -a /etc/sysctl.d/99-coredump.conf

# Test
ulimit -c unlimited
bash -c 'kill -ABRT $$'
ls -la /tmp/cores/

