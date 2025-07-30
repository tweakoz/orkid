#!/usr/bin/env python3
"""
CDN Log Monitor - Real-time monitoring of CDN operations
"""

import sys
import os
import time
import json
from pathlib import Path
from datetime import datetime
import subprocess
import signal

class CDNLogMonitor:
    def __init__(self, log_dir="./logs"):
        self.log_dir = Path(log_dir)
        self.running = True
        self.stats = {
            'uploads': {'total': 0, 'success': 0, 'failed': 0, 'bytes': 0},
            'downloads': {'total': 0, 'success': 0, 'failed': 0, 'bytes': 0},
            'auth': {'attempts': 0, 'success': 0, 'failed': 0}
        }
        
        # Set up signal handler for clean shutdown
        signal.signal(signal.SIGINT, self.signal_handler)
        signal.signal(signal.SIGTERM, self.signal_handler)
    
    def signal_handler(self, signum, frame):
        print("\nShutting down log monitor...")
        self.running = False
    
    def print_stats(self):
        """Print current statistics"""
        print("\n" + "="*60)
        print(f"CDN Statistics - {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print("="*60)
        
        # Upload stats
        uploads = self.stats['uploads']
        if uploads['total'] > 0:
            success_rate = (uploads['success'] / uploads['total']) * 100
            print(f"Uploads:   {uploads['total']:4d} total, {uploads['success']:4d} success, "
                  f"{uploads['failed']:4d} failed ({success_rate:.1f}% success)")
            print(f"           {uploads['bytes']/1024/1024:.1f} MB uploaded")
        else:
            print("Uploads:   No activity")
        
        # Download stats
        downloads = self.stats['downloads']
        if downloads['total'] > 0:
            success_rate = (downloads['success'] / downloads['total']) * 100
            print(f"Downloads: {downloads['total']:4d} total, {downloads['success']:4d} success, "
                  f"{downloads['failed']:4d} failed ({success_rate:.1f}% success)")
            print(f"           {downloads['bytes']/1024/1024:.1f} MB downloaded")
        else:
            print("Downloads: No activity")
        
        # Auth stats
        auth = self.stats['auth']
        if auth['attempts'] > 0:
            success_rate = (auth['success'] / auth['attempts']) * 100
            print(f"Auth:      {auth['attempts']:4d} attempts, {auth['success']:4d} success, "
                  f"{auth['failed']:4d} failed ({success_rate:.1f}% success)")
        else:
            print("Auth:      No activity")
    
    def parse_error_log_line(self, line):
        """Parse nginx error log line"""
        # Format: 2024/01/01 12:00:00 [info] 123#0: message
        try:
            parts = line.split(': ', 1)
            if len(parts) < 2:
                return None
                
            header = parts[0]
            message = parts[1]
            
            # Extract log level
            level_start = header.find('[')
            level_end = header.find(']')
            if level_start == -1 or level_end == -1:
                return None
                
            level = header[level_start+1:level_end]
            
            return {
                'level': level,
                'message': message.strip(),
                'raw': line.strip()
            }
        except:
            return None
    
    def process_log_entry(self, entry):
        """Process a single log entry"""
        if not entry:
            return
            
        message = entry['message']
        level = entry['level']
        
        # Auth events
        if "Auth attempt:" in message:
            self.stats['auth']['attempts'] += 1
            print(f"🔐 {message}")
            
        elif "Auth success:" in message:
            self.stats['auth']['success'] += 1
            print(f"✅ {message}")
            
        elif "Auth failed" in message:
            self.stats['auth']['failed'] += 1
            print(f"❌ {message}")
            
        # Upload events
        elif "Upload started:" in message:
            self.stats['uploads']['total'] += 1
            print(f"⬆️  {message}")
            
        elif "Upload completed:" in message:
            self.stats['uploads']['success'] += 1
            # Extract size from message
            size_match = message.split('size:')
            if len(size_match) > 1:
                try:
                    size = int(size_match[1].split()[0])
                    self.stats['uploads']['bytes'] += size
                except:
                    pass
            print(f"✅ {message}")
            
        elif "Upload failed" in message or "Upload blocked" in message:
            self.stats['uploads']['failed'] += 1
            print(f"❌ {message}")
            
        # Download events
        elif "Download completed:" in message:
            self.stats['downloads']['total'] += 1
            self.stats['downloads']['success'] += 1
            # Extract size from message
            size_match = message.split('size:')
            if len(size_match) > 1:
                try:
                    size = int(size_match[1].split()[0])
                    self.stats['downloads']['bytes'] += size
                except:
                    pass
            print(f"⬇️  {message}")
            
        elif "Download failed:" in message:
            self.stats['downloads']['total'] += 1
            self.stats['downloads']['failed'] += 1
            print(f"❌ {message}")
            
        # Error events
        elif level in ['error', 'crit', 'alert', 'emerg']:
            print(f"🚨 ERROR: {message}")
            
        elif level == 'warn':
            print(f"⚠️  WARNING: {message}")
    
    def follow_log_file(self, log_file):
        """Follow a log file like 'tail -f'"""
        try:
            with open(log_file, 'r') as f:
                # Seek to end of file
                f.seek(0, 2)
                
                while self.running:
                    line = f.readline()
                    if line:
                        entry = self.parse_error_log_line(line)
                        self.process_log_entry(entry)
                    else:
                        time.sleep(0.1)
        except FileNotFoundError:
            print(f"Log file not found: {log_file}")
            print("Make sure the CDN container is running and logs are mounted.")
        except KeyboardInterrupt:
            pass
    
    def run(self):
        """Main monitoring loop"""
        error_log = self.log_dir / "error.log"
        
        print("CDN Log Monitor Started")
        print(f"Monitoring: {error_log}")
        print("Press Ctrl+C to stop")
        print("Legend: 🔐=Auth 📁=File ⬆️=Upload ⬇️=Download ✅=Success ❌=Failed ⚠️=Warning 🚨=Error")
        
        # Print initial stats
        self.print_stats()
        
        # Start monitoring
        stats_timer = 0
        
        if not error_log.exists():
            print(f"\nWaiting for log file: {error_log}")
            while not error_log.exists() and self.running:
                time.sleep(1)
        
        if self.running:
            print(f"\nStarting to monitor {error_log}...")
            
            # Use subprocess to tail the file
            try:
                proc = subprocess.Popen(['tail', '-f', str(error_log)], 
                                      stdout=subprocess.PIPE, 
                                      stderr=subprocess.PIPE,
                                      universal_newlines=True)
                
                while self.running:
                    line = proc.stdout.readline()
                    if line:
                        entry = self.parse_error_log_line(line)
                        self.process_log_entry(entry)
                    
                    # Print stats every 30 seconds
                    stats_timer += 1
                    if stats_timer >= 300:  # Roughly 30 seconds (depends on log activity)
                        self.print_stats()
                        stats_timer = 0
                
                proc.terminate()
                
            except FileNotFoundError:
                print("'tail' command not found, falling back to Python implementation")
                self.follow_log_file(error_log)
        
        # Final stats
        self.print_stats()

def main():
    if len(sys.argv) > 1:
        log_dir = sys.argv[1]
    else:
        log_dir = "./logs"
    
    monitor = CDNLogMonitor(log_dir)
    monitor.run()

if __name__ == "__main__":
    main()