#!/usr/bin/env python3
################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################

from orkengine import core
import time
import os
import sys
import subprocess
import threading

VERBOSE = False

################################################################
# Test 1: Basic performance benchmark (single process, threads)
################################################################

def test_basic_benchmark():
    """Test basic send/receive performance using benchmark methods"""
    
    
    NUM_BYTES = 1024 * 1024  # 1MB
    
    # Run benchmark in threads
    send_complete = threading.Event()
    recv_complete = threading.Event()
    sender = [None]
    receiver = [None]
    
    def send_thread():
        sender[0] = core.ipcq.Sender("test_py_bench")
        sender[0].benchSendPerformance(NUM_BYTES)
        send_complete.set()
        
    def recv_thread():
        receiver[0] = core.ipcq.Receiver("test_py_bench")
        receiver[0].benchReceivePerformance(NUM_BYTES)
        recv_complete.set()
    
    # Start threads
    t1 = threading.Thread(target=recv_thread)
    t2 = threading.Thread(target=send_thread)
    
    t1.start()
    time.sleep(0.1)  # Let receiver start first
    t2.start()
    
    # Wait for completion
    send_ok = send_complete.wait(timeout=10)
    recv_ok = recv_complete.wait(timeout=10)
    
    t1.join()
    t2.join()
    
    if send_ok and recv_ok:
        print("PASS: Basic benchmark test")
        return True
    else:
        print("FAIL: Basic benchmark test - timeout")
        return False

################################################################
# Test 2: DataBlock transfer
################################################################

def test_datablock_transfer():
    """Test DataBlock send/receive"""
    
    
    # Create test data
    test_str1 = "Hello from Python IPCQ test"
    test_str2 = "This is a DataBlock transfer"
    
    # Send DataBlock
    dblock = core.DataBlock()
    dblock.writeString(test_str1)
    dblock.writeString(test_str2)
    
    if VERBOSE:
        print("Sending DataBlock:", dblock.hexdump())
    
    # Use threads to avoid deadlock
    recv_result = [None]
    sender = [None]
    receiver = [None]
    
    def recv_thread():
        receiver[0] = core.ipcq.Receiver("test_py_datablock")
        recv_result[0] = receiver[0].receiveDataBlock()
    
    def send_thread():
        sender[0] = core.ipcq.Sender("test_py_datablock")
        sender[0].sendDataBlock(dblock)
    
    t1 = threading.Thread(target=recv_thread)
    t2 = threading.Thread(target=send_thread)
    t1.start()
    t2.start()
    
    t1.join(timeout=5)
    t2.join(timeout=5)
    
    if recv_result[0] is None:
        print("FAIL: DataBlock transfer - timeout")
        return False
    
    # Verify received data
    received = recv_result[0]
    stream = core.DataBlockInputStream(received)
    
    str1 = stream.readString()
    str2 = stream.readString()
    
    if str1 == test_str1 and str2 == test_str2:
        print("PASS: DataBlock transfer")
        if VERBOSE:
            print("  Received:", str1, str2)
        return True
    else:
        print("FAIL: DataBlock transfer - data mismatch")
        print(f"  Expected: '{test_str1}', '{test_str2}'")
        print(f"  Got: '{str1}', '{str2}'")
        return False

################################################################
# Test 3: Inter-process communication
################################################################

def test_interprocess():
    """Test communication between separate processes"""
    
    
    # Create a simple receiver script
    receiver_script = """
from orkengine import core
import time
core.coreappinit()

receiver = core.ipcq.Receiver("SHM_PYTEST")

# Receive 1MB
receiver.benchReceivePerformance(1024*1024)

# Receive DataBlock
dblock = receiver.receiveDataBlock()
stream = core.DataBlockInputStream(dblock)
print("RECEIVED:", stream.readString())
"""
    
    # Write receiver script to temp file
    with open("/tmp/ipcq_receiver_test.py", "w") as f:
        f.write(receiver_script)
    
    # Start receiver process
    proc = subprocess.Popen(
        [sys.executable, "/tmp/ipcq_receiver_test.py"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )
    
    time.sleep(1)  # Let receiver start
    
    # Send from main process
    sender = core.ipcq.Sender("SHM_PYTEST")
    
    # Send benchmark data
    sender.benchSendPerformance(1024*1024)
    
    # Send DataBlock
    dblock = core.DataBlock()
    dblock.writeString("Inter-process test SUCCESS")
    sender.sendDataBlock(dblock)
    
    # Wait for receiver to complete
    stdout, stderr = proc.communicate(timeout=10)
    
    # Clean up
    os.unlink("/tmp/ipcq_receiver_test.py")
    
    # Check result
    output = stdout.decode('utf-8')
    if "RECEIVED: Inter-process test SUCCESS" in output:
        print("PASS: Inter-process communication")
        return True
    else:
        print("FAIL: Inter-process communication")
        print("stdout:", output)
        print("stderr:", stderr.decode('utf-8'))
        return False

################################################################
# Test 4: Multiple channels
################################################################

def test_multiple_channels():
    """Test multiple simultaneous channels"""
    
    
    NUM_CHANNELS = 3
    NUM_BYTES = 100 * 1024  # 100KB per channel
    
    # Create multiple channels
    senders = []
    receivers = []
    
    # Create channels in separate threads to avoid deadlock
    def create_channel(i):
        receiver = core.ipcq.Receiver(f"SHM_CHAN{i}")
        sender = core.ipcq.Sender(f"SHM_CHAN{i}")
        return (sender, receiver)
    
    # Create all channels in parallel
    import concurrent.futures
    with concurrent.futures.ThreadPoolExecutor(max_workers=NUM_CHANNELS) as executor:
        futures = [executor.submit(create_channel, i) for i in range(NUM_CHANNELS)]
        for i, future in enumerate(concurrent.futures.as_completed(futures)):
            sender, receiver = future.result()
            senders.append(sender)
            receivers.append(receiver)
    
    # Run benchmarks on all channels simultaneously
    threads = []
    results = [False] * NUM_CHANNELS * 2
    
    def send_thread(idx, sender):
        try:
            sender.benchSendPerformance(NUM_BYTES)
            results[idx] = True
        except Exception as e:
            print(f"Send thread {idx} error: {e}")
    
    def recv_thread(idx, receiver):
        try:
            receiver.benchReceivePerformance(NUM_BYTES)
            results[NUM_CHANNELS + idx] = True
        except Exception as e:
            print(f"Recv thread {idx} error: {e}")
    
    # Start all threads
    for i in range(NUM_CHANNELS):
        t1 = threading.Thread(target=recv_thread, args=(i, receivers[i]))
        t2 = threading.Thread(target=send_thread, args=(i, senders[i]))
        threads.extend([t1, t2])
        t1.start()
        
    time.sleep(0.1)  # Let receivers start
    
    for i in range(1, NUM_CHANNELS * 2, 2):
        threads[i].start()
    
    # Wait for all threads
    for t in threads:
        t.join(timeout=10)
    
    # Check results
    if all(results):
        print(f"PASS: Multiple channels ({NUM_CHANNELS} simultaneous)")
        return True
    else:
        print(f"FAIL: Multiple channels - some threads failed")
        for i, r in enumerate(results):
            if not r:
                print(f"  Thread {i} failed")
        return False

################################################################
# Main test runner
################################################################

def run_all_tests():
    """Run all IPCQ Python tests"""
    
    print("=== IPCQ Python Integration Tests ===")
    
    # Initialize core
    core.coreappinit()
    
    tests = [
        ("Basic Benchmark", test_basic_benchmark),
        ("DataBlock Transfer", test_datablock_transfer),
        ("Inter-process Communication", test_interprocess),
        ("Multiple Channels", test_multiple_channels),
    ]
    
    passed = 0
    failed = 0
    
    for name, test_func in tests:
        print(f"\nRunning: {name}")
        try:
            if test_func():
                passed += 1
            else:
                failed += 1
        except Exception as e:
            print(f"FAIL: {name} - Exception: {e}")
            import traceback
            traceback.print_exc()
            failed += 1
    
    print(f"\n=== Summary ===")
    print(f"Passed: {passed}")
    print(f"Failed: {failed}")
    print(f"Total:  {len(tests)}")
    
    return failed == 0

if __name__ == "__main__":
    # Set verbose mode from command line
    if "--verbose" in sys.argv:
        VERBOSE = True
        
    success = run_all_tests()
    sys.exit(0 if success else 1)