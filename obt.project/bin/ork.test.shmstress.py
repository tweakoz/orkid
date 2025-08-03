#!/usr/bin/env python3

import sys
import time
import subprocess
import os
from ork import path as ork_path

# Add scripts path for obt modules
sys.path.append(str(ork_path.scripts))

def print_separator(title, char="=", width=80):
    """Print a formatted separator with title"""
    print()
    print(char * width)
    print(f" {title} ".center(width, char))
    print(char * width)
    print()

def print_test_header(test_name, config):
    """Print test header with configuration details"""
    print(f"🧪 TEST: {test_name}")
    print(f"   Configuration: {config}")
    print(f"   Started at: {time.strftime('%H:%M:%S')}")
    print("-" * 60)

def run_stress_test(test_name, args, expected_success=True):
    """Run a single stress test configuration"""
    config_str = " ".join(args) if args else "default"
    print_test_header(test_name, config_str)
    
    # Build command
    cmd = ["_ork.shmobject.stress.exe"] + args
    
    try:
        # Run the test
        start_time = time.time()
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
        end_time = time.time()
        
        # Check result
        success = result.returncode == 0
        duration = end_time - start_time
        
        if success and expected_success:
            print(f"✅ PASSED ({duration:.2f}s)")
        elif not success and not expected_success:
            print(f"⚠️  EXPECTED FAILURE ({duration:.2f}s)")
        else:
            print(f"❌ FAILED ({duration:.2f}s)")
            print("STDOUT:", result.stdout[-500:] if result.stdout else "None")
            print("STDERR:", result.stderr[-500:] if result.stderr else "None")
            return False
            
        # Show key metrics from output
        if result.stdout:
            lines = result.stdout.split('\n')
            for line in lines:
                if "Workers succeeded:" in line or "Workers failed:" in line:
                    print(f"   {line.strip()}")
                elif "Processes:" in line and "Computations:" in line:
                    print(f"   {line.strip()}")
                elif "🎉" in line or "❌" in line:
                    print(f"   {line.strip()}")
        
        return success
        
    except subprocess.TimeoutExpired:
        print("❌ TIMEOUT (120s limit exceeded)")
        return False
    except Exception as e:
        print(f"❌ ERROR: {e}")
        return False

def main():
    """Run comprehensive ShmObject stress test suite"""
    
    print_separator("ORKID SHMOBJECT STRESS TEST SUITE")
    print("This suite tests the bulletproof ShmObject implementation")
    print("with various configurations to validate atomic competition,")
    print("synchronization, and scalability under different conditions.")
    print()
    print(f"Test executable: _ork.shmobject.stress.exe")
    print(f"Started at: {time.strftime('%Y-%m-%d %H:%M:%S')}")
    
    # Test configurations: (name, args, expected_success)
    test_configs = [
        # Basic functionality tests
        ("Default Configuration", [], True),
        ("Minimal Load", ["-p", "2", "-w", "500", "-d", "100"], True),
        ("Quick Test", ["-p", "4", "-w", "1000", "-d", "500", "-c", "2000"], True),
        
        # Concurrency stress tests
        ("Medium Concurrency", ["-p", "8", "-w", "2000", "-d", "1000"], True),
        ("High Concurrency", ["-p", "16", "-w", "1500", "-d", "2000"], True),
        ("Extreme Concurrency", ["-p", "32", "-w", "1000", "-d", "3000"], True),
        
        # Timing variation tests
        ("Low Timing Variance", ["-p", "8", "-w", "2000", "-d", "100"], True),
        ("High Timing Variance", ["-p", "12", "-w", "1500", "-d", "5000"], True),
        ("Chaotic Timing", ["-p", "20", "-w", "800", "-d", "4000", "-c", "4000"], True),
        
        # Duration stress tests
        ("Short Duration", ["-p", "10", "-w", "500", "-d", "1000", "-c", "2000"], True),
        ("Long Duration", ["-p", "6", "-w", "4000", "-d", "1500", "-c", "8000"], True),
        
        # Edge cases
        ("Single Process", ["-p", "1", "-w", "1000", "-d", "0"], True),
        ("No Startup Delay", ["-p", "16", "-w", "1000", "-d", "0"], True),
        ("Very Short Work", ["-p", "12", "-w", "200", "-d", "1000"], True),
        
        # Scalability limits
        ("High Process Count", ["-p", "48", "-w", "500", "-d", "2000", "-c", "3000"], True),
        ("Maximum Concurrency", ["-p", "64", "-w", "300", "-d", "1000", "-c", "2000"], True),
    ]
    
    # Run all tests
    total_tests = len(test_configs)
    passed_tests = 0
    failed_tests = 0
    start_time = time.time()
    
    for i, (test_name, args, expected_success) in enumerate(test_configs, 1):
        print_separator(f"TEST {i}/{total_tests}: {test_name}", "-")
        
        success = run_stress_test(test_name, args, expected_success)
        if success:
            passed_tests += 1
        else:
            failed_tests += 1
        
        # Brief pause between tests to let system settle
        if i < total_tests:
            time.sleep(1)
    
    # Summary
    end_time = time.time()
    total_duration = end_time - start_time
    
    print_separator("TEST SUITE SUMMARY")
    print(f"Total tests run: {total_tests}")
    print(f"Tests passed: {passed_tests}")
    print(f"Tests failed: {failed_tests}")
    print(f"Success rate: {(passed_tests/total_tests)*100:.1f}%")
    print(f"Total duration: {total_duration:.1f} seconds")
    print(f"Average per test: {total_duration/total_tests:.1f} seconds")
    print()
    
    if failed_tests == 0:
        print("🎉 ALL TESTS PASSED!")
        print("ShmObject atomic competition is working perfectly!")
        print("The system handles all concurrency scenarios robustly.")
        return 0
    else:
        print(f"❌ {failed_tests} TEST(S) FAILED!")
        print("Some configurations did not pass. Review the output above.")
        return 1

if __name__ == "__main__":
    sys.exit(main())