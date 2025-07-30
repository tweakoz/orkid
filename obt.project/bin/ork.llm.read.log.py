#!/usr/bin/env python3
"""
Read and parse Claude conversation log files (.jsonl format)
"""

import json
import argparse
import os
from datetime import datetime
from pathlib import Path

def format_timestamp(ts_str):
    """Convert ISO timestamp to readable format"""
    try:
        dt = datetime.fromisoformat(ts_str.replace('Z', '+00:00'))
        return dt.strftime("%Y-%m-%d %H:%M:%S")
    except:
        return ts_str

def parse_time_arg(time_str):
    """Parse time argument in format HH:MM or YYYY-MM-DD HH:MM"""
    try:
        # Try full datetime first
        if ' ' in time_str:
            return datetime.strptime(time_str, "%Y-%m-%d %H:%M")
        # Try just time (assume today)
        else:
            time_part = datetime.strptime(time_str, "%H:%M").time()
            return datetime.combine(datetime.today(), time_part)
    except ValueError:
        raise argparse.ArgumentTypeError(f"Invalid time format: {time_str}. Use HH:MM or YYYY-MM-DD HH:MM")

def is_within_time_range(timestamp_str, start_time, end_time):
    """Check if timestamp is within the specified range"""
    try:
        dt = datetime.fromisoformat(timestamp_str.replace('Z', '+00:00'))
        # If no date in time args, use the date from the timestamp
        if start_time and start_time.date() == datetime.today().date():
            start_time = datetime.combine(dt.date(), start_time.time())
        if end_time and end_time.date() == datetime.today().date():
            end_time = datetime.combine(dt.date(), end_time.time())
        
        if start_time and dt < start_time:
            return False
        if end_time and dt > end_time:
            return False
        return True
    except:
        return True  # If can't parse, include it

def print_message(entry, show_tools=False, show_assistant=True, show_user=True, start_time=None, end_time=None):
    """Print a single message entry"""
    msg_type = entry.get('type', '')
    timestamp_str = entry.get('timestamp', '')
    
    # Check time range
    if not is_within_time_range(timestamp_str, start_time, end_time):
        return False
    
    if msg_type == 'user' and show_user:
        # User message
        msg = entry.get('message', {})
        content = msg.get('content', '')
        timestamp = format_timestamp(timestamp_str)
        
        print(f"\n[{timestamp}] USER:")
        if isinstance(content, str):
            print(f"  {content}")
        elif isinstance(content, list):
            for item in content:
                if isinstance(item, dict) and item.get('type') == 'text':
                    print(f"  {item.get('text', '')}")
                elif isinstance(item, dict) and item.get('type') == 'tool_result':
                    if show_tools:
                        print(f"  [Tool Result: {item.get('tool_use_id', '')}]")
        print("-" * 80)
        return True
        
    elif msg_type == 'assistant' and show_assistant:
        # Assistant message
        msg = entry.get('message', {})
        content = msg.get('content', [])
        timestamp = format_timestamp(timestamp_str)
        
        print(f"\n[{timestamp}] ASSISTANT:")
        for item in content:
            if isinstance(item, dict):
                if item.get('type') == 'text':
                    text = item.get('text', '')
                    # Truncate very long responses
                    if len(text) > 500 and not args.full:
                        print(f"  {text[:500]}...")
                        print(f"  [Truncated - use --full to see complete response]")
                    else:
                        print(f"  {text}")
                elif item.get('type') == 'tool_use' and show_tools:
                    tool_name = item.get('name', '')
                    print(f"  [Tool Use: {tool_name}]")
        return True
    
    return False

def file_contains_time_range(log_file, start_time, end_time):
    """Check if file contains any messages in the given time range"""
    try:
        with open(log_file, 'r') as f:
            for line in f:
                try:
                    entry = json.loads(line.strip())
                    timestamp_str = entry.get('timestamp', '')
                    if is_within_time_range(timestamp_str, start_time, end_time):
                        return True
                except json.JSONDecodeError:
                    continue
    except:
        pass
    return False

def search_messages(log_file, search_term=None, show_tools=False, show_assistant=True, show_user=True, start_time=None, end_time=None, show_file_header=True):
    """Search and display messages from a log file"""
    if show_file_header:
        print(f"\nReading: {log_file}")
        print("=" * 80)
    
    count = 0
    with open(log_file, 'r') as f:
        for line in f:
            try:
                entry = json.loads(line.strip())
                
                # Skip if searching and term not found
                if search_term:
                    entry_str = json.dumps(entry).lower()
                    if search_term.lower() not in entry_str:
                        continue
                
                # Print the message
                if print_message(entry, show_tools, show_assistant, show_user, start_time, end_time):
                    count += 1
                
            except json.JSONDecodeError:
                continue
    
    if show_file_header:
        print(f"\nTotal messages shown: {count}")
    
    return count

def list_sessions(claude_dir):
    """List all available session files"""
    projects_dir = Path(claude_dir) / "projects"
    if not projects_dir.exists():
        print(f"No projects directory found at {projects_dir}")
        return
    
    print("\nAvailable Claude sessions:")
    print("-" * 80)
    
    for project_dir in sorted(projects_dir.iterdir()):
        if project_dir.is_dir():
            print(f"\nProject: {project_dir.name}")
            jsonl_files = list(project_dir.glob("*.jsonl"))
            
            for jsonl_file in sorted(jsonl_files, key=lambda x: x.stat().st_mtime, reverse=True):
                mtime = datetime.fromtimestamp(jsonl_file.stat().st_mtime)
                size_mb = jsonl_file.stat().st_size / (1024 * 1024)
                print(f"  {jsonl_file.name} - {mtime.strftime('%Y-%m-%d %H:%M')} - {size_mb:.1f}MB")

def main():
    global args
    parser = argparse.ArgumentParser(description="Read Claude conversation logs")
    parser.add_argument("file", nargs="?", help="Path to .jsonl file to read")
    parser.add_argument("-s", "--search", help="Search for specific term")
    parser.add_argument("-t", "--tools", action="store_true", help="Show tool usage")
    parser.add_argument("-n", "--no-assistant", action="store_true", help="Hide assistant responses")
    parser.add_argument("-f", "--full", action="store_true", help="Show full responses without truncation")
    parser.add_argument("-l", "--list", action="store_true", help="List all available sessions")
    parser.add_argument("-r", "--recent", action="store_true", help="Open most recent session")
    parser.add_argument("--show", nargs=2, metavar=("START", "END"), help="Show messages between start and end times (HH:MM or YYYY-MM-DD HH:MM). Automatically searches all files.")
    parser.add_argument("--show-user", action="store_true", help="Show only user messages (use with --show)")
    parser.add_argument("--show-claude", action="store_true", help="Show only assistant messages (use with --show)")
    
    args = parser.parse_args()
    
    claude_dir = Path.home() / ".claude"
    
    # Parse time arguments if provided
    start_time = None
    end_time = None
    if args.show:
        start_time = parse_time_arg(args.show[0])
        end_time = parse_time_arg(args.show[1])
    
    # Determine what to show
    show_user = True
    show_assistant = not args.no_assistant
    if args.show_user or args.show_claude:
        # If specific flags are set, only show those
        show_user = args.show_user
        show_assistant = args.show_claude
    
    if args.list:
        list_sessions(claude_dir)
        return
    
    # If --show is used without a file, search all files
    if args.show and not args.file:
        projects_dir = claude_dir / "projects"
        all_files = []
        for project_dir in projects_dir.iterdir():
            if project_dir.is_dir():
                all_files.extend(project_dir.glob("*.jsonl"))
        
        # Sort files by modification time for consistent ordering
        all_files.sort(key=lambda x: x.stat().st_mtime)
        
        print(f"Searching for messages between {args.show[0]} and {args.show[1]}...")
        total_count = 0
        files_with_matches = []
        
        for log_file in all_files:
            # Quick check if file contains messages in range
            if file_contains_time_range(log_file, start_time, end_time):
                files_with_matches.append(log_file)
        
        if files_with_matches:
            print(f"Found messages in {len(files_with_matches)} file(s)")
            for log_file in files_with_matches:
                count = search_messages(log_file, args.search, args.tools, show_assistant, show_user, start_time, end_time)
                total_count += count
            
            print(f"\nTotal messages across all files: {total_count}")
        else:
            print("No messages found in the specified time range")
        return
    
    if args.recent:
        # Find most recent file
        projects_dir = claude_dir / "projects"
        all_files = []
        for project_dir in projects_dir.iterdir():
            if project_dir.is_dir():
                all_files.extend(project_dir.glob("*.jsonl"))
        
        if all_files:
            recent_file = max(all_files, key=lambda x: x.stat().st_mtime)
            print(f"Opening most recent: {recent_file}")
            search_messages(recent_file, args.search, args.tools, show_assistant, show_user, start_time, end_time)
        else:
            print("No conversation files found")
        return
    
    if not args.file:
        print("Please specify a file to read or use --list to see available sessions")
        parser.print_help()
        return
    
    log_file = Path(args.file)
    if not log_file.exists():
        # Try to find it in the claude directory
        possible_paths = [
            log_file,
            claude_dir / "projects" / "-Users-michael-projects-orkid" / log_file.name,
            claude_dir / "projects" / "*" / log_file.name,
        ]
        
        found = False
        for path in possible_paths:
            if "*" in str(path):
                matches = list(Path(str(path).split("*")[0]).parent.glob(str(path).split("/")[-1]))
                if matches:
                    log_file = matches[0]
                    found = True
                    break
            elif path.exists():
                log_file = path
                found = True
                break
        
        if not found:
            print(f"File not found: {args.file}")
            return
    
    search_messages(log_file, args.search, args.tools, show_assistant, show_user, start_time, end_time)

if __name__ == "__main__":
    main()