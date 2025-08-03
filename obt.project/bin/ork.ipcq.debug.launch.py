#!/usr/bin/env python3

import sys
import time
import subprocess
import os
from ork import path as ork_path

# Add scripts path for obt modules
sys.path.append(str(ork_path.scripts))

from obt.tmux import Session

def main():
    """Launch debug IPCQ test with producer and consumer."""
    
    # Check for background mode
    background_mode = "-b" in sys.argv
    
    if background_mode:
        # Background mode: run processes and redirect to files
        output_dir = "/tmp/ipcq_debug_test"
        os.makedirs(output_dir, exist_ok=True)
        
        consumer_log = f"{output_dir}/consumer.log"
        producer_log = f"{output_dir}/producer.log"
        
        print(f"Running IPCQ Debug Test in background mode")
        print(f"Consumer output: {consumer_log}")
        print(f"Producer output: {producer_log}")
        
        # Start consumer in background
        consumer_file = open(consumer_log, "w")
        consumer_proc = subprocess.Popen(
            ["_ork.ipcq.debug.exe", "consumer"],
            stdout=consumer_file,
            stderr=subprocess.STDOUT
        )
        
        # Give consumer time to start
        time.sleep(1)
        
        # Start producer in background
        producer_file = open(producer_log, "w")
        producer_proc = subprocess.Popen(
            ["_ork.ipcq.debug.exe", "producer"],
            stdout=producer_file,
            stderr=subprocess.STDOUT
        )
        
        # Wait for both processes to complete
        print("Waiting for test to complete...")
        
        # Poll with timeout instead of wait to avoid hanging
        timeout = 60  # 60 seconds timeout
        start_time = time.time()
        
        while time.time() - start_time < timeout:
            producer_done = producer_proc.poll() is not None
            consumer_done = consumer_proc.poll() is not None
            
            if producer_done and consumer_done:
                break
                
            # Flush files to ensure output is written
            consumer_file.flush()
            producer_file.flush()
            time.sleep(0.5)
        
        # Get exit codes
        producer_result = producer_proc.returncode
        consumer_result = consumer_proc.returncode
        
        # Close files after processes complete
        consumer_file.close()
        producer_file.close()
        
        # Kill processes if they're still running (timeout)
        if producer_proc.poll() is None:
            producer_proc.kill()
            print("Producer process timed out and was killed")
        if consumer_proc.poll() is None:
            consumer_proc.kill()
            print("Consumer process timed out and was killed")
        
        print(f"Producer exit code: {producer_result}")
        print(f"Consumer exit code: {consumer_result}")
        
        # Show tail of logs
        print("\n=== Last 10 lines of consumer log ===")
        subprocess.run(["tail", "-10", consumer_log])
        print("\n=== Last 10 lines of producer log ===")
        subprocess.run(["tail", "-10", producer_log])
        
    else:
        # Tmux mode: original behavior
        session = Session("ipcq_debug_test", orientation="horizontal")
        
        # Launch consumer in first pane
        consumer_cmd = "echo 'Starting IPCQ Debug Consumer...'; _ork.ipcq.debug.exe consumer; echo 'process exited, waiting....'; sleep 300"
        session.first_command([consumer_cmd])
        
        # Launch producer in second pane
        producer_cmd = "sleep 1; echo 'Starting IPCQ Debug Producer (1M messages)...'; _ork.ipcq.debug.exe producer; echo 'process exited, waiting....'; sleep 300"
        session.next_command([producer_cmd])
        
        # Execute the session
        print("Launching IPCQ Debug Test in tmux session 'ipcq_debug_test'")
        print("This will send 1 million messages with END marker for clean termination")
        print("To kill session: tmux kill-session -t ipcq_debug_test")
        session.execute()

if __name__ == "__main__":
    main()