#!/usr/bin/env python3
"""Measure clock offset between this host and a remote host over SSH.

Uses a persistent SSH connection to minimize RTT, then takes multiple
NTP-style measurements to compute offset statistics.

Usage:
    ork.hosttimediff.py --host skylix
    ork.hosttimediff.py --host skylix --samples 20 --interval 0.5
"""

import argparse
import subprocess
import time
import sys
import os
import tempfile

def get_local_ms():
    """Current epoch time in milliseconds."""
    return int(time.time() * 1000)

def measure_once(host, control_path):
    """Single NTP-style offset measurement via persistent SSH."""
    t1 = get_local_ms()
    result = subprocess.run(
        ["ssh", "-o", f"ControlPath={control_path}", host, "date +%s%3N"],
        capture_output=True, text=True, timeout=10)
    t3 = get_local_ms()
    if result.returncode != 0:
        return None
    t2 = int(result.stdout.strip())
    rtt = t3 - t1
    offset = t2 - (t1 + t3) // 2
    return {"t1": t1, "t2": t2, "t3": t3, "rtt": rtt, "offset": offset}

def main():
    parser = argparse.ArgumentParser(description="Measure clock offset to a remote host")
    parser.add_argument("--host", required=True, help="SSH hostname")
    parser.add_argument("--samples", type=int, default=10, help="Number of samples (default: 10)")
    parser.add_argument("--interval", type=float, default=1.0, help="Seconds between samples (default: 1.0)")
    args = parser.parse_args()

    control_path = os.path.join(tempfile.gettempdir(), f"ssh-timediff-{args.host}")

    # Establish persistent SSH connection
    print(f"Establishing SSH connection to {args.host}...")
    setup = subprocess.run(
        ["ssh", "-o", "ControlMaster=yes", "-o", f"ControlPath={control_path}",
         "-o", "ControlPersist=120", args.host, "true"],
        capture_output=True, timeout=30)
    if setup.returncode != 0:
        print(f"Failed to connect to {args.host}", file=sys.stderr)
        sys.exit(1)

    print(f"Taking {args.samples} samples at {args.interval}s intervals...\n")

    samples = []
    for i in range(args.samples):
        m = measure_once(args.host, control_path)
        if m:
            samples.append(m)
            print(f"  [{i+1:2d}/{args.samples}] offset={m['offset']:+d}ms  rtt={m['rtt']}ms")
        else:
            print(f"  [{i+1:2d}/{args.samples}] FAILED")
        if i < args.samples - 1:
            time.sleep(args.interval)

    # Teardown persistent connection
    subprocess.run(
        ["ssh", "-o", f"ControlPath={control_path}", "-O", "exit", args.host],
        capture_output=True)

    if not samples:
        print("\nNo successful measurements.", file=sys.stderr)
        sys.exit(1)

    offsets = [s["offset"] for s in samples]
    rtts = [s["rtt"] for s in samples]

    avg_offset = sum(offsets) / len(offsets)
    avg_rtt = sum(rtts) / len(rtts)
    min_offset = min(offsets)
    max_offset = max(offsets)
    min_rtt = min(rtts)
    max_rtt = max(rtts)

    # Standard deviation
    variance = sum((o - avg_offset) ** 2 for o in offsets) / len(offsets)
    stddev = variance ** 0.5

    # The sample with lowest RTT has the best accuracy
    best = min(samples, key=lambda s: s["rtt"])

    print(f"\n{'='*50}")
    print(f"  Host:         {args.host}")
    print(f"  Samples:      {len(samples)}/{args.samples}")
    print(f"{'='*50}")
    print(f"  Offset avg:   {avg_offset:+.1f}ms")
    print(f"  Offset range: {min_offset:+d}ms to {max_offset:+d}ms")
    print(f"  Offset stdev: {stddev:.1f}ms")
    print(f"  Best sample:  {best['offset']:+d}ms (rtt={best['rtt']}ms)")
    print(f"{'='*50}")
    print(f"  RTT avg:      {avg_rtt:.1f}ms")
    print(f"  RTT range:    {min_rtt}ms to {max_rtt}ms")
    print(f"{'='*50}")
    print(f"\n  Use ORK_AUDIOSWEEP_OFFSET={-best['offset']} on generator to compensate")

if __name__ == "__main__":
    main()
