#!/usr/bin/env sh
# 
sudo mkdir -p /cores
sudo chmod 1777 /cores
sudo sysctl -w kern.coredump=1
sudo sysctl -w kern.corefile=/cores/core.%N.%P
ulimit -c unlimited
