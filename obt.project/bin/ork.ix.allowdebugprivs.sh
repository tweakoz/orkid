#!/usr/bin/env sh

echo "Enabling Debug Privileges.."
echo '1' | sudo tee /proc/sys/kernel/perf_event_paranoid