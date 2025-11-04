#!/usr/bin/env python3
# .github/scripts/sanitize.py

import sys
import os
import re
import subprocess
import platform

class LogSanitizer:
    def __init__(self):
        self.system = platform.system()
        self.home = os.path.expanduser("~")
        self.user = os.environ.get('USER', 'unknown')
        self.tmpdir = os.environ.get('TMPDIR', '/tmp')
        self.build_tmpdir = os.environ.get('BUILD_TMPDIR', '/tmp')
        
        # Platform-specific paths
        if self.system == 'Darwin':  # macOS
            home_pattern = r'/Users/[^/\s]+'
            home_replacement = '***'
        else:  # Linux
            home_pattern = r'/home/[^/\s]+'
            home_replacement = '***'
        
        self.replacements = [
            # Literal replacements
            (self.home, home_replacement),
            (self.user, 'USER'),
            (self.tmpdir, '/tmp/BUILD_DIR'),
            (self.build_tmpdir, '/tmp/BUILD_DIR'),
            
            # Regex patterns (r prefix)
            (f'r{home_pattern}', home_replacement),
            (r'r/private/tmp/[^\s]+', '/tmp/BUILD_DIR'),  # macOS temp
            (r'r/var/folders/[^\s]+', '/tmp/BUILD_DIR'),  # macOS temp
            (r'rstaging-\d+', 'staging-XXXX'),
            (r'rbuild-\d{10}', 'build-TIMESTAMP'),
            (r'r/tmp/tmp[a-zA-Z0-9_]+', '/tmp/tmpXXXX'),
            (r'rvenv/lib/python\d+\.\d+', 'venv/lib/pythonX.X'),
            (r'r(API_KEY|TOKEN|PASSWORD|SECRET)=[^\s]+', r'\1=REDACTED'),
        ]
        
        # Compile patterns
        self.patterns = []
        for pattern, replacement in self.replacements:
            if pattern.startswith('r'):
                # Regex pattern
                pattern = pattern[1:]  # Remove 'r' prefix
                self.patterns.append((re.compile(pattern), replacement))
            else:
                # Literal string - escape for regex
                self.patterns.append((re.compile(re.escape(pattern)), replacement))
    
    def filter_line(self, line):
        for pattern, replacement in self.patterns:
            line = pattern.sub(replacement, line)
        return line
    
    def run_command(self, cmd):
        # Force unbuffered output
        env = os.environ.copy()
        env['PYTHONUNBUFFERED'] = '1'
        
        process = subprocess.Popen(
            cmd,
            shell=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            universal_newlines=True,
            bufsize=1,  # Line buffered
            env=env
        )
        
        # Read and filter output line by line
        while True:
            line = process.stdout.readline()
            if not line and process.poll() is not None:
                break
            if line:
                filtered = self.filter_line(line.rstrip('\n'))
                print(filtered, flush=True)
        
        return process.poll()

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: sanitize.py <command>", file=sys.stderr)
        sys.exit(1)
    
    sanitizer = LogSanitizer()
    cmd = ' '.join(sys.argv[1:])
    sys.exit(sanitizer.run_command(cmd))