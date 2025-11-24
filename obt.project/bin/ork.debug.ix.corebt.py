#!/usr/bin/env python3
"""
Core dump backtrace analyzer
Dumps callstack from a core file using GDB
"""

import argparse
import subprocess
import sys
import os
import shutil
from pathlib import Path

class Colors:
    """ANSI color codes for terminal output"""
    RED = '\033[91m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    MAGENTA = '\033[95m'
    CYAN = '\033[96m'
    WHITE = '\033[97m'
    RESET = '\033[0m'
    BOLD = '\033[1m'

def find_executable(exe_name):
    """Find executable in PATH or return as-is if it's a full path"""
    # If it's already a path, check if it exists
    if os.path.sep in exe_name:
        if os.path.isfile(exe_name) and os.access(exe_name, os.X_OK):
            return os.path.abspath(exe_name)
        else:
            return None
    
    # Search in PATH
    exe_path = shutil.which(exe_name)
    if exe_path:
        return os.path.abspath(exe_path)
    
    # Also check current directory
    local_path = os.path.join(os.getcwd(), exe_name)
    if os.path.isfile(local_path) and os.access(local_path, os.X_OK):
        return os.path.abspath(local_path)
    
    return None

def analyze_core(corefile, executable, verbose=False, all_threads=False, full=False):
    """Analyze core dump using GDB"""
    
    # Build GDB command
    gdb_commands = [
        "set pagination off",
        "set print thread-events off"
    ]
    
    if verbose:
        gdb_commands.append("echo \\n=== CRASH LOCATION ===\\n")
    
    if full:
        gdb_commands.append("bt full")
    else:
        gdb_commands.append("bt")
    
    if all_threads:
        if verbose:
            gdb_commands.append("echo \\n=== ALL THREADS ===\\n")
        gdb_commands.append("thread apply all bt")
    
    if verbose:
        gdb_commands.extend([
            "echo \\n=== SIGNAL INFO ===\\n",
            "print $_siginfo",
            "echo \\n=== REGISTERS ===\\n", 
            "info registers",
            "echo \\n=== LOADED LIBRARIES ===\\n",
            "info sharedlibrary"
        ])
    
    # Build full GDB command
    gdb_args = ["gdb", "-batch", "-quiet"]
    for cmd in gdb_commands:
        gdb_args.extend(["-ex", cmd])
    gdb_args.extend([executable, corefile])
    
    try:
        result = subprocess.run(
            gdb_args,
            capture_output=True,
            text=True,
            timeout=30
        )
        
        return result.stdout, result.stderr, result.returncode
        
    except subprocess.TimeoutExpired:
        return None, "GDB timeout after 30 seconds", 1
    except Exception as e:
        return None, f"Failed to run GDB: {e}", 1

def get_core_info(corefile):
    """Get basic info about core file using 'file' command"""
    try:
        result = subprocess.run(
            ["file", corefile],
            capture_output=True,
            text=True,
            timeout=5
        )
        if result.returncode == 0:
            return result.stdout.strip()
    except:
        pass
    return None

def main():
    parser = argparse.ArgumentParser(
        description="Analyze core dumps and extract backtraces",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s -c core.12345 -e myprogram
  %(prog)s -c /tmp/cores/core.dump -e python3 -v
  %(prog)s -c corefile -e ./build/bin/app --all-threads --full
        """
    )
    
    parser.add_argument("-c", "--core", 
                       required=True,
                       help="Path to core dump file")
    parser.add_argument("-e", "--exe",
                       required=True, 
                       help="Executable name (searched in PATH) or full path")
    parser.add_argument("-v", "--verbose",
                       action="store_true",
                       help="Show detailed information (registers, libraries, etc)")
    parser.add_argument("-a", "--all-threads",
                       action="store_true",
                       help="Show backtrace for all threads")
    parser.add_argument("-f", "--full",
                       action="store_true",
                       help="Show full backtrace with local variables")
    parser.add_argument("--no-color",
                       action="store_true",
                       help="Disable colored output")
    parser.add_argument("-o", "--output",
                       help="Save output to file")
    
    args = parser.parse_args()
    
    # Disable colors if requested or if not in terminal
    if args.no_color or not sys.stdout.isatty():
        for color in dir(Colors):
            if not color.startswith('_'):
                setattr(Colors, color, '')
    
    # Verify core file exists
    if not os.path.isfile(args.core):
        print(f"{Colors.RED}Error: Core file not found: {args.core}{Colors.RESET}", file=sys.stderr)
        sys.exit(1)
    
    # Find executable
    exe_path = find_executable(args.exe)
    if not exe_path:
        print(f"{Colors.RED}Error: Executable not found: {args.exe}{Colors.RESET}", file=sys.stderr)
        print(f"Searched in: PATH and current directory", file=sys.stderr)
        sys.exit(1)
    
    print(f"{Colors.CYAN}Core file:{Colors.RESET} {args.core}")
    print(f"{Colors.CYAN}Executable:{Colors.RESET} {exe_path}")
    
    # Get core file info
    core_info = get_core_info(args.core)
    if core_info:
        # Extract just the relevant part
        if "from '" in core_info:
            from_exe = core_info.split("from '")[1].split("'")[0]
            print(f"{Colors.YELLOW}Core from:{Colors.RESET} {from_exe}")
            # Warn if mismatch
            if os.path.basename(from_exe) != os.path.basename(exe_path):
                print(f"{Colors.RED}Warning: Core appears to be from different executable!{Colors.RESET}")
    
    print(f"{Colors.CYAN}{'='*60}{Colors.RESET}")
    
    # Analyze core
    stdout, stderr, returncode = analyze_core(
        args.core, 
        exe_path,
        verbose=args.verbose,
        all_threads=args.all_threads,
        full=args.full
    )
    
    if stdout:
        # Colorize output
        output = stdout
        if sys.stdout.isatty() and not args.no_color:
            # Highlight important parts
            output = output.replace("Thread", f"{Colors.GREEN}Thread{Colors.RESET}")
            output = output.replace("#0 ", f"{Colors.BOLD}#0 {Colors.RESET}")
            output = output.replace("Program terminated with signal", 
                                  f"{Colors.RED}Program terminated with signal{Colors.RESET}")
            output = output.replace("SIGSEGV", f"{Colors.RED}SIGSEGV{Colors.RESET}")
            output = output.replace("SIGABRT", f"{Colors.RED}SIGABRT{Colors.RESET}")
            output = output.replace("SIGBUS", f"{Colors.RED}SIGBUS{Colors.RESET}")
            output = output.replace("SIGFPE", f"{Colors.RED}SIGFPE{Colors.RESET}")
            output = output.replace("SIGILL", f"{Colors.RED}SIGILL{Colors.RESET}")
        
        print(output)
        
        # Save to file if requested
        if args.output:
            # Strip colors for file output
            clean_output = stdout
            with open(args.output, 'w') as f:
                f.write(f"Core file: {args.core}\n")
                f.write(f"Executable: {exe_path}\n")
                f.write("="*60 + "\n")
                f.write(clean_output)
            print(f"{Colors.GREEN}Output saved to: {args.output}{Colors.RESET}")
    
    if stderr and stderr.strip():
        print(f"{Colors.YELLOW}GDB warnings/errors:{Colors.RESET}", file=sys.stderr)
        print(stderr, file=sys.stderr)
    
    if returncode != 0 and not stdout:
        print(f"{Colors.RED}Failed to analyze core dump{Colors.RESET}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
