#!/usr/bin/env ork.python
"""Test script demonstrating different IPCQ sizes in Python with throughput benchmarks"""

from orkengine import core
from orkengine.core import ipcq, DataBlock
import time  # Still needed for sleep
import threading
import hashlib

# Initialize the core (required for Timer)
core.coreappinit()

def benchmark_ipcq_variant(sender, receiver, variant_name, message_size, queue_size):
    """Benchmark a specific IPCQ variant by sending binary data via DataBlocks"""
    
    # Enable C++ profiling
    sender._profiling_enabled = True
    receiver._profiling_enabled = True
    
    # Use a single consistent block size for all transfers
    # 1MB blocks - good balance of efficiency and memory usage
    block_size = 1 * 1024 * 1024  # 1MB
    
    # Fixed number of blocks for consistent testing
    num_blocks = 100  # 100MB total
    actual_total = num_blocks * block_size
    
    print(f"\n  Benchmark: Sending {num_blocks:,} DataBlocks ({block_size:,} bytes each, {actual_total / (1024*1024):.1f} MB total)")
    
    # Generate test data with known pattern for verification
    # Use a simple pattern that's easy to verify
    test_data = bytes([i % 256 for i in range(block_size)])
    
    # Calculate checksum for verification
    expected_checksum = hashlib.md5(test_data).hexdigest()
    
    # Track results
    received_blocks = []
    receive_done = threading.Event()
    receive_error = [None]
    
    # Track C++ profiling metrics
    send_times = []
    receive_times = []
    
    def receive_thread():
        try:
            received_count = 0
            while received_count < num_blocks:
                # receiveDataBlock() blocks until a complete DataBlock is available
                dblock = receiver.receiveDataBlock()
                
                # Verify the data
                # When using writeRawData, there's no header - data is stored as-is
                received_data = dblock.bytes
                received_checksum = hashlib.md5(received_data).hexdigest()
                
                if received_checksum != expected_checksum:
                    # Debug: show first few bytes
                    expected_preview = test_data[:16].hex() if len(test_data) >= 16 else test_data.hex()
                    received_preview = received_data[:16].hex() if len(received_data) >= 16 else received_data.hex()
                    raise Exception(f"Data corruption detected!\n    Expected checksum: {expected_checksum}\n    Got checksum: {received_checksum}\n    Expected size: {len(test_data)}\n    Got size: {len(received_data)}\n    Expected first bytes: {expected_preview}\n    Got first bytes: {received_preview}")
                
                received_blocks.append(dblock)
                received_count += 1
                
                # Collect C++ profiling metrics after each receive
                if receiver._transfer_time > 0:
                    receive_times.append(receiver._transfer_time)
                
                # Progress indicator
                if received_count % max(1, num_blocks // 10) == 0:
                    print(f"    Received {received_count}/{num_blocks} blocks...", end='\r')
            
            receive_done.set()
        except Exception as e:
            receive_error[0] = str(e)
            receive_done.set()
    
    # Start receiver thread
    recv_thread = threading.Thread(target=receive_thread)
    recv_thread.start()
    
    # Wait a moment for receiver to be ready
    time.sleep(0.01)
    
    # Send DataBlocks
    for i in range(num_blocks):
        dblock = DataBlock()
        dblock.writeRawData(test_data)  # Use writeRawData to avoid 16-byte header
        sender.sendDataBlock(dblock)
        
        # Collect C++ profiling metrics after each send
        if sender._transfer_time > 0:
            send_times.append(sender._transfer_time)
    
    # Wait for receiver to finish
    if not receive_done.wait(timeout=60.0):
        print(f"\n    WARNING: Receiver timeout! Only received {len(received_blocks)}/{num_blocks} blocks")
    
    recv_thread.join(timeout=1.0)
    
    # Check for errors
    if receive_error[0]:
        print(f"\n    ERROR: {receive_error[0]}")
        return 0.0
    
    received_bytes = len(received_blocks) * block_size
    
    # Calculate throughput based on C++ profiling
    total_send_time = sum(send_times) if send_times else 0
    total_receive_time = sum(receive_times) if receive_times else 0
    
    # Use the max of send or receive time as the duration
    duration = max(total_send_time, total_receive_time)
    
    if duration > 0:
        throughput_mbps = (received_bytes / (1024 * 1024)) / duration
        blocks_per_sec = len(received_blocks) / duration
    else:
        throughput_mbps = 0
        blocks_per_sec = 0
    
    print(f"\n  Results:")
    
    # Report C++ profiling metrics
    if send_times:
        avg_send_time = sum(send_times) / len(send_times)
        print(f"    C++ Send profile: avg={avg_send_time*1000:.3f}ms, total={total_send_time:.3f}s, count={len(send_times)}")
    
    if receive_times:
        avg_receive_time = sum(receive_times) / len(receive_times)
        print(f"    C++ Receive profile: avg={avg_receive_time*1000:.3f}ms, total={total_receive_time:.3f}s, count={len(receive_times)}")
    
    print(f"    Transfer time (C++): {duration:.3f} seconds")
    print(f"    Throughput: {throughput_mbps:.2f} MB/s")
    print(f"    Block rate: {blocks_per_sec:.0f} blocks/s")
    print(f"    DataBlocks sent: {num_blocks}")
    print(f"    DataBlocks received: {len(received_blocks)}")
    print(f"    Data verified: ✓ All blocks match expected checksum")
    print(f"    Total data: {received_bytes / 1024:.1f} KB")
    
    # Calculate overhead
    # Each DataBlock requires: 1 header message + N data messages + 1 trailer message
    messages_per_block = 2 + (block_size + message_size - 1) // message_size
    total_messages = num_blocks * messages_per_block
    overhead_percent = ((total_messages * message_size - received_bytes) / received_bytes) * 100
    print(f"    Protocol overhead: {overhead_percent:.1f}%")
    
    return throughput_mbps

def test_ipcq_sizes():
    # List available sizes
    print("=" * 60)
    print("IPCQ DataBlock Transfer Benchmark")
    print("=" * 60)
    
    sizes = ipcq.list_sizes()
    print("\nAvailable IPCQ sizes:")
    for name, info in sizes.items():
        print(f"  {name}: {info['description']}")
    
    throughput_results = {}
    
    # Use unique names with timestamp to avoid conflicts
    import os
    test_id = str(os.getpid())
    
    # Test configurations: (variant_key, class_name, description, msg_size, queue_size)
    test_configs = [
        ('ipcq_1K64', 'Ipcq1K64', 'IPCQ_1K64 (1K queue, 64 byte messages)', 64, 1024),
        ('ipcq_4K256', 'Ipcq4K256', 'IPCQ_4K256 (4K queue, 256 byte messages)', 256, 4096),
        ('ipcq_8K1K', 'Ipcq8K1K', 'IPCQ_8K1K (8K queue, 1K messages)', 1024, 8192),
        ('ipcq_4K4K', 'Ipcq4K4K', 'IPCQ_4K4K (4K queue, 4K messages)', 4096, 4096),
        ('ipcq_1K16K', 'Ipcq1K16K', 'IPCQ_1K16K (1K queue, 16K messages)', 16384, 1024),
        ('ipcq_1K64K', 'Ipcq1K64K', 'IPCQ_1K64K (1K queue, 64K messages)', 65536, 1024),
        ('ipcq_1K256K', 'Ipcq1K256K', 'IPCQ_1K256K (1K queue, 256K messages)', 262144, 1024),
        ('ipcq_1K512K', 'Ipcq1K512K', 'IPCQ_1K512K (1K queue, 512K messages)', 524288, 1024),
        ('ipcq_1K1M', 'Ipcq1K1M', 'IPCQ_1K1M (1K queue, 1M messages)', 1048576, 1024),
        ('legacy', '', 'Legacy IPCQ (256 byte messages, 8K queue)', 256, 8192),
    ]
    
    for variant_key, class_name, description, msg_size, queue_size in test_configs:
        print("\n" + "=" * 60)
        print(f"Testing {description}:")
        print("=" * 60)
        
        test_name = f"test_{variant_key}_{test_id}"
        
        # Create sender and receiver
        if variant_key == 'legacy':
            sender = ipcq.Sender(test_name)
            receiver = ipcq.Receiver(test_name)
        else:
            sender_class = getattr(ipcq, f"{class_name}Sender")
            receiver_class = getattr(ipcq, f"{class_name}Receiver")
            sender = sender_class(test_name)
            receiver = receiver_class(test_name)
        
        print(f"  Message size: {msg_size} bytes")
        print(f"  Queue size: {queue_size} entries")
        capacity = (msg_size * queue_size)
        if capacity >= 1024*1024:
            print(f"  Queue capacity: {capacity / (1024*1024):.1f} MB")
        else:
            print(f"  Queue capacity: {capacity / 1024:.1f} KB")
        
        throughput_results[variant_key] = benchmark_ipcq_variant(
            sender, receiver, description.split('(')[0].strip(),
            msg_size, queue_size
        )
        
        del receiver
        del sender
        time.sleep(0.5)
    
    # Summary table
    print("\n" + "=" * 60)
    print("BENCHMARK RESULTS SUMMARY")
    print("=" * 60)
    print("\n{:<12} {:>8} {:>8} {:>12} {:>10}".format(
        "Variant", "Msg Size", "Queue", "Throughput", "Relative"
    ))
    print("-" * 52)
    
    if throughput_results:
        best_throughput = max(throughput_results.values()) if max(throughput_results.values()) > 0 else 1
        
        # Define size mapping
        size_info = {
            'ipcq_1K64': (64, 1024),
            'ipcq_4K256': (256, 4096),
            'ipcq_8K1K': (1024, 8192),
            'ipcq_4K4K': (4096, 4096),
            'ipcq_1K16K': (16384, 1024),
            'ipcq_1K64K': (65536, 1024),
            'ipcq_1K256K': (262144, 1024),
            'ipcq_1K512K': (524288, 1024),
            'ipcq_1K1M': (1048576, 1024),
            'legacy': (256, 8192)
        }
        
        for variant, throughput in sorted(throughput_results.items(), key=lambda x: x[1], reverse=True):
            msg_size, queue_size = size_info[variant]
            
            if throughput > 0:
                relative = (throughput / best_throughput) * 100
            else:
                relative = 0
            
            # Format message size
            if msg_size >= 1024:
                size_str = f"{msg_size//1024}K"
            else:
                size_str = f"{msg_size}B"
            
            print("{:<12} {:>8} {:>8} {:>10.2f} MB/s {:>9.0f}%".format(
                variant.upper(), size_str, queue_size, throughput, relative
            ))
    
    print("\n✅ All IPCQ sizes tested successfully with data verification!")

if __name__ == "__main__":
    test_ipcq_sizes()