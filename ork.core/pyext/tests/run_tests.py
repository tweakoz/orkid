#!/usr/bin/env python3

"""
Test runner for ork.core Python bindings.

This script runs all tests for:
1. Logger bindings (test_logger.py)
2. NotCurses UI bindings (test_ncui.py)
3. Integration tests (test_integration.py)
4. Performance/stress tests
5. Manual examples

Usage:
    python run_tests.py [options]

Options:
    --unit          Run only unit tests (default)
    --stress        Run stress tests
    --integration   Run integration tests
    --examples      Run example demonstrations
    --all           Run all tests and examples
    --verbose       Enable verbose output
    --help          Show this help
"""

import sys
import os
import time
import subprocess
import argparse
from pathlib import Path

# Add test directory to path
test_dir = Path(__file__).parent
sys.path.insert(0, str(test_dir))
sys.path.insert(0, str(test_dir.parent / 'pyfiles'))

# Color output for terminals
class Colors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def print_header(text):
    """Print a colored header"""
    print(f"\n{Colors.HEADER}{Colors.BOLD}{'='*60}{Colors.ENDC}")
    print(f"{Colors.HEADER}{Colors.BOLD}{text.center(60)}{Colors.ENDC}")
    print(f"{Colors.HEADER}{Colors.BOLD}{'='*60}{Colors.ENDC}\n")

def print_success(text):
    """Print success message"""
    print(f"{Colors.OKGREEN}✓ {text}{Colors.ENDC}")

def print_warning(text):
    """Print warning message"""
    print(f"{Colors.WARNING}⚠ {text}{Colors.ENDC}")

def print_error(text):
    """Print error message"""
    print(f"{Colors.FAIL}✗ {text}{Colors.ENDC}")

def print_info(text):
    """Print info message"""
    print(f"{Colors.OKCYAN}ℹ {text}{Colors.ENDC}")

def check_modules():
    """Check if required modules are available"""
    print_info("Checking module availability...")
    
    modules_status = {}
    
    try:
        import ork.core as core
        modules_status['ork.core'] = True
        print_success("ork.core module: Available")
    except ImportError as e:
        modules_status['ork.core'] = False
        print_error(f"ork.core module: Not available ({e})")
    
    try:
        import ork.core.ncui as ncui
        modules_status['ork.core.ncui'] = True
        print_success("ork.core.ncui module: Available")
    except ImportError as e:
        modules_status['ork.core.ncui'] = False
        print_error(f"ork.core.ncui module: Not available ({e})")
    
    try:
        import ork.core.pyfiles.ncui_cleanup as ncui_cleanup
        modules_status['ncui_cleanup'] = True
        print_success("ncui_cleanup utilities: Available")
    except ImportError as e:
        modules_status['ncui_cleanup'] = False
        print_warning(f"ncui_cleanup utilities: Not available ({e})")
    
    return modules_status

def run_unit_tests(verbose=False):
    """Run unit tests"""
    print_header("Running Unit Tests")
    
    test_files = [
        'test_logger.py',
        'test_ncui.py'
    ]
    
    results = {}
    
    for test_file in test_files:
        test_path = test_dir / test_file
        if not test_path.exists():
            print_error(f"Test file not found: {test_file}")
            results[test_file] = False
            continue
        
        print_info(f"Running {test_file}...")
        
        try:
            # Import and run the test
            if test_file == 'test_logger.py':
                import test_logger
                # Run tests programmatically
                import unittest
                suite = unittest.TestLoader().loadTestsFromModule(test_logger)
                runner = unittest.TextTestRunner(verbosity=2 if verbose else 1)
                result = runner.run(suite)
                results[test_file] = result.wasSuccessful()
                
            elif test_file == 'test_ncui.py':
                import test_ncui
                import unittest
                suite = unittest.TestLoader().loadTestsFromModule(test_ncui)
                runner = unittest.TextTestRunner(verbosity=2 if verbose else 1)
                result = runner.run(suite)
                results[test_file] = result.wasSuccessful()
                
        except Exception as e:
            print_error(f"Error running {test_file}: {e}")
            results[test_file] = False
    
    # Summary
    print_info("Unit Test Summary:")
    for test_file, success in results.items():
        if success:
            print_success(f"{test_file}: PASSED")
        else:
            print_error(f"{test_file}: FAILED")
    
    return all(results.values())

def run_integration_tests(verbose=False):
    """Run integration tests"""
    print_header("Running Integration Tests")
    
    try:
        import test_integration
        import unittest
        
        suite = unittest.TestLoader().loadTestsFromModule(test_integration)
        runner = unittest.TextTestRunner(verbosity=2 if verbose else 1)
        result = runner.run(suite)
        
        if result.wasSuccessful():
            print_success("Integration tests: PASSED")
        else:
            print_error("Integration tests: FAILED")
        
        return result.wasSuccessful()
        
    except Exception as e:
        print_error(f"Error running integration tests: {e}")
        return False

def run_stress_tests(verbose=False):
    """Run stress tests"""
    print_header("Running Stress Tests")
    
    stress_functions = []
    
    try:
        import test_logger
        stress_functions.append(('Logger Stress Test', test_logger.run_logger_stress_test))
    except ImportError:
        print_warning("Logger stress test not available")
    
    try:
        import test_ncui
        stress_functions.append(('UI Stress Test', test_ncui.run_ui_stress_test))
    except ImportError:
        print_warning("UI stress test not available")
    
    try:
        import test_integration
        stress_functions.append(('Integration Stress Test', test_integration.run_integration_stress_test))
    except ImportError:
        print_warning("Integration stress test not available")
    
    results = {}
    
    for name, func in stress_functions:
        print_info(f"Running {name}...")
        try:
            start_time = time.time()
            func()
            elapsed = time.time() - start_time
            print_success(f"{name} completed in {elapsed:.2f} seconds")
            results[name] = True
        except Exception as e:
            print_error(f"{name} failed: {e}")
            results[name] = False
    
    return all(results.values()) if results else True

def run_examples(verbose=False):
    """Run example demonstrations"""
    print_header("Running Example Demonstrations")
    
    examples_dir = test_dir.parent / 'pyfiles'
    example_files = [
        'example_logger.py',
        'example_ncui.py'
    ]
    
    results = {}
    
    for example_file in example_files:
        example_path = examples_dir / example_file
        if not example_path.exists():
            print_warning(f"Example file not found: {example_file}")
            continue
        
        print_info(f"Available examples in {example_file}:")
        
        # Show available examples without running them
        try:
            with open(example_path, 'r') as f:
                content = f.read()
                
            # Extract function definitions that look like examples
            import re
            functions = re.findall(r'def (\w+_example)\(\)', content)
            for func in functions:
                print(f"  - {func.replace('_', ' ').title()}")
            
            print_info(f"To run examples: python {example_file} <example_name>")
            results[example_file] = True
            
        except Exception as e:
            print_error(f"Error reading {example_file}: {e}")
            results[example_file] = False
    
    return all(results.values()) if results else True

def main():
    """Main test runner"""
    parser = argparse.ArgumentParser(description='Test runner for ork.core Python bindings')
    parser.add_argument('--unit', action='store_true', help='Run unit tests')
    parser.add_argument('--stress', action='store_true', help='Run stress tests')
    parser.add_argument('--integration', action='store_true', help='Run integration tests')
    parser.add_argument('--examples', action='store_true', help='Show available examples')
    parser.add_argument('--all', action='store_true', help='Run all tests')
    parser.add_argument('--verbose', '-v', action='store_true', help='Enable verbose output')
    
    args = parser.parse_args()
    
    # Default to unit tests if no specific tests requested
    if not any([args.unit, args.stress, args.integration, args.examples, args.all]):
        args.unit = True
    
    print_header("ork.core Python Bindings Test Suite")
    
    # Check module availability
    modules_status = check_modules()
    
    if not any(modules_status.values()):
        print_error("No ork.core modules available. Please build the project first.")
        print_info("Build instructions:")
        print_info("1. Configure and build the C++ project")
        print_info("2. Make sure Python bindings are enabled")
        print_info("3. Install the built module")
        return 1
    
    # Track overall results
    all_passed = True
    
    # Run requested tests
    if args.unit or args.all:
        if not run_unit_tests(args.verbose):
            all_passed = False
    
    if args.integration or args.all:
        if not run_integration_tests(args.verbose):
            all_passed = False
    
    if args.stress or args.all:
        if not run_stress_tests(args.verbose):
            all_passed = False
    
    if args.examples or args.all:
        if not run_examples(args.verbose):
            all_passed = False
    
    # Final summary
    print_header("Test Suite Summary")
    
    if all_passed:
        print_success("All tests PASSED!")
        print_info("The ork.core Python bindings are working correctly.")
    else:
        print_error("Some tests FAILED!")
        print_info("Check the output above for details.")
    
    # Additional information
    print("\nAdditional Information:")
    print_info("Test files location: " + str(test_dir))
    print_info("Example files location: " + str(test_dir.parent / 'pyfiles'))
    
    if modules_status.get('ork.core', False):
        print_info("Logger functionality: Available")
    
    if modules_status.get('ork.core.ncui', False):
        print_info("NotCurses UI functionality: Available")
    
    if modules_status.get('ncui_cleanup', False):
        print_info("Cleanup utilities: Available")
    
    return 0 if all_passed else 1

if __name__ == "__main__":
    exit_code = main()
    sys.exit(exit_code) 