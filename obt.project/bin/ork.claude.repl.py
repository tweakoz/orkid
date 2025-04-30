#!/usr/bin/env python3 

import os
import glob
import cmd
import argparse
import anthropic
from typing import List, Dict, Optional
from obt import path, pathtools
import json
import time
import re
import fnmatch

ork_dir = path.orkid()
print(f"ork_dir: {ork_dir}")

class CodeAgentRepl(cmd.Cmd):
    prompt = "orkid-agent> "
    intro = "Welcome to Orkid Code Agent REPL. Type 'help' for commands."
    
    def __init__(self, api_key: str):
        super().__init__()
        self.client = anthropic.Anthropic(api_key=api_key)
        self.codebase_path = os.path.abspath(ork_dir/"ork.core")
        self.file_cache: Dict[str, str] = {}
        self.index = []  # Lightweight index of files
        self.selected_files = []  # Currently selected files
        self.last_api_call = 0  # Timestamp for rate limiting
        
        # Conversation state
        self.conversation = []
        self.files_sent = False
        self.session_files = []
        
        # Initialize by scanning codebase
        self.scan_codebase()
    
    def scan_codebase(self):
        """Scan the codebase and build an index of files."""
        print(f"Scanning codebase at {self.codebase_path}...")
        self.files = []
        for ext in ['.py', '.cpp', '.h', '.hpp', '.c']:
            self.files.extend(glob.glob(f"{self.codebase_path}/**/*{ext}", recursive=True))
        
        # Build a simple index
        self.index = []
        for file_path in self.files:
            rel_path = os.path.relpath(file_path, self.codebase_path)
            size = os.path.getsize(file_path)
            _, ext = os.path.splitext(file_path)
            self.index.append({
                "path": rel_path,
                "size": size,
                "type": ext[1:] if ext else "unknown"
            })
        
        # Sort by path
        self.index.sort(key=lambda x: x["path"])
        print(f"Indexed {len(self.index)} files.")
    
    def read_file(self, filepath: str) -> str:
        """Read a file and cache its content."""
        abs_path = os.path.join(self.codebase_path, filepath) if not os.path.isabs(filepath) else filepath
        
        if abs_path in self.file_cache:
            return self.file_cache[abs_path]
        
        try:
            with open(abs_path, 'r', encoding='utf-8', errors='replace') as file:
                content = file.read()
                self.file_cache[abs_path] = content
                return content
        except Exception as e:
            return f"Error reading file: {e}"
    
    def do_list(self, arg):
        """List all indexed files or first N files: list [N]"""
        try:
            n = int(arg) if arg else 20
        except ValueError:
            n = 20
            
        for i, entry in enumerate(self.index[:n]):
            print(f"{i+1}. {entry['path']} ({entry['size']} bytes)")
        if len(self.index) > n:
            print(f"... and {len(self.index) - n} more files (use 'search' to find specific files)")
    
    def do_search(self, arg):
        """Search for files in codebase: search filename_pattern"""
        if not arg:
            print("Please provide a search pattern")
            return
            
        results = [entry for entry in self.index if arg.lower() in entry["path"].lower()]
        for i, entry in enumerate(results[:20]):
            print(f"{i+1}. {entry['path']} ({entry['size']} bytes)")
        if len(results) > 20:
            print(f"... and {len(results) - 20} more matches")
        if not results:
            print("No matching files found")
    
    def do_view(self, arg):
        """View contents of a file: view path/to/file.py"""
        if not arg:
            print("Please specify a file path")
            return
        
        content = self.read_file(arg)
        print(f"Contents of {arg}:")
        print("-" * 80)
        print(content)
        print("-" * 80)
    
    def has_wildcards(self, path):
        """Check if a path contains wildcard characters."""
        return '*' in path or '?' in path or '[' in path
    
    def do_select(self, arg):
        """Select file(s) to include in Claude conversation: select path/to/file.py
        Supports wildcards: select *.cpp or select src/*/*.h"""
        if not arg:
            print("Please specify a file path or pattern")
            return
        
        # Check if pattern has wildcards
        if self.has_wildcards(arg):
            # Find all matching files
            matched_files = []
            for entry in self.index:
                if fnmatch.fnmatch(entry["path"], arg):
                    matched_files.append(entry["path"])
            
            if not matched_files:
                print(f"No files match pattern: {arg}")
                return
            
            # Add all matching files
            for file_path in matched_files:
                if file_path not in self.selected_files:
                    self.selected_files.append(file_path)
            
            print(f"Added {len(matched_files)} files matching '{arg}' to selection.")
        else:
            # Regular file selection
            # Check if file exists
            full_path = os.path.join(self.codebase_path, arg)
            if not os.path.exists(full_path):
                print(f"File not found: {arg}")
                return
            
            # Add to selected files if not already there
            if arg not in self.selected_files:
                self.selected_files.append(arg)
                print(f"Added {arg} to selected files.")
            else:
                print(f"{arg} is already selected.")
        
        # Show current selection
        print(f"Currently selected files ({len(self.selected_files)}):")
        for i, file in enumerate(self.selected_files[:5]):
            print(f"{i+1}. {file}")
        if len(self.selected_files) > 5:
            print(f"... and {len(self.selected_files) - 5} more (use 'selected' to see all)")
            
        # If selection changed, reset the conversation
        if self.files_sent and set(self.selected_files) != set(self.session_files):
            print("Note: Selection changed. Next question will start a new conversation.")
            self.files_sent = False
    
    def complete_select(self, text, line, begidx, endidx):
        """Provide path completion for the select command."""
        # Extract the path prefix from the command line
        args = line.split()
        if len(args) > 1:
            # If there's already a partial path, use it as prefix
            prefix = args[-1] if text == '' else args[-1][:-len(text)]
        else:
            prefix = ""
        
        # Find all file paths that match the prefix and text
        matches = []
        for entry in self.index:
            path = entry["path"]
            if path.startswith(prefix + text):
                # Return the part after the prefix
                path_suggestion = path[len(prefix):]
                # If there's a directory separator in the suggestion, only complete up to it
                next_slash = path_suggestion.find(os.sep)
                if next_slash != -1:
                    path_suggestion = path_suggestion[:next_slash + 1]
                if path_suggestion not in matches:
                    matches.append(path_suggestion)
        
        return matches
    
    def do_selected(self, arg):
        """Show currently selected files"""
        if not self.selected_files:
            print("No files currently selected. Use 'select' to add files.")
            return
        
        print(f"Currently selected files ({len(self.selected_files)}):")
        for i, file in enumerate(self.selected_files):
            print(f"{i+1}. {file}")
    
    def do_reset(self, arg):
        """Reset the conversation with Claude (starts fresh)"""
        self.conversation = []
        self.files_sent = False
        print("Conversation reset. The next question will start a new conversation.")
    
    def do_clear_selection(self, arg):
        """Clear all selected files"""
        self.selected_files = []
        self.files_sent = False
        print("Selected files cleared. The next question will start a new conversation.")
    
    def do_ask(self, arg):
        """Ask Claude about selected files: ask your question here"""
        if not arg:
            print("Please provide a question")
            return
            
        if not self.selected_files:
            print("No files selected. Use 'select' to add files for context.")
            return
        
        # Check rate limiting
        current_time = time.time()
        if current_time - self.last_api_call < 3:  # Add a 3-second delay between calls
            wait_time = 3 - (current_time - self.last_api_call)
            print(f"Rate limiting: waiting {wait_time:.1f} seconds...")
            time.sleep(wait_time)
        
        # If we haven't sent files yet or the selection changed, start a new conversation
        if not self.files_sent:
            self.start_new_conversation()
        
        # Add the user's question to the conversation
        self.conversation.append({
            "role": "user",
            "content": arg
        })
        
        print("Sending question to Claude... (this may take a moment)")
        try:
            self.last_api_call = time.time()
            response = self.client.messages.create(
                model="claude-3-7-sonnet-20250219",
                max_tokens=4096,
                messages=self.conversation
            )
            
            answer = response.content[0].text
            # Add Claude's response to the conversation history
            self.conversation.append({
                "role": "assistant",
                "content": answer
            })
            
            print("\nClaude's response:")
            print(answer)
        except Exception as e:
            print(f"Error communicating with Claude: {e}")
            if "rate_limit" in str(e):
                print("\nTIP: You're hitting rate limits. Try:")
                print("1. Selecting fewer files with 'select'")
                print("2. Waiting a minute before trying again")
                print("3. Using 'view' to inspect files locally first")
    
    def start_new_conversation(self):
        """Start a new conversation with Claude, sending all selected files."""
        self.conversation = []
        
        # Build the initial message with code context
        message = "I'm going to share code from my project, then ask questions about it. Here's the code:\n\n"
        
        # Add selected files
        file_count = 0
        for filepath in self.selected_files:
            content = self.read_file(filepath)
            if "Error" not in content:
                message += f"File: {filepath}\n```\n{content}\n```\n\n"
                file_count += 1
            else:
                print(f"Warning: Couldn't read {filepath}: {content}")
        
        message += "Please analyze these files. I'll ask specific questions next."
        
        # Add the message to the conversation
        self.conversation.append({
            "role": "user",
            "content": message
        })
        
        print(f"Sending {file_count} files to Claude... (this may take a moment)")
        try:
            self.last_api_call = time.time()
            response = self.client.messages.create(
                model="claude-3-7-sonnet-20250219",
                max_tokens=4096,
                messages=self.conversation
            )
            
            answer = response.content[0].text
            # Add Claude's response to the conversation history
            self.conversation.append({
                "role": "assistant",
                "content": answer
            })
            
            # Mark files as sent and save the current selection
            self.files_sent = True
            self.session_files = self.selected_files.copy()
            
            print("\nClaude's initial analysis:")
            print(answer)
        except Exception as e:
            print(f"Error communicating with Claude: {e}")
            if "rate_limit" in str(e):
                print("\nTIP: You're hitting rate limits. Try:")
                print("1. Selecting fewer files with 'select'")
                print("2. Waiting a minute before trying again")
                print("3. Using 'view' to inspect files locally first")
    
    def do_status(self, arg):
        """Show current conversation status"""
        if not self.conversation:
            print("No active conversation. Ask a question to start one.")
            return
        
        print(f"Active conversation with {len(self.conversation)} messages")
        print(f"Files sent: {self.files_sent}")
        if self.files_sent:
            print(f"Session includes {len(self.session_files)} files")
        print(f"Selected files: {len(self.selected_files)}")
        
        if self.files_sent and set(self.selected_files) != set(self.session_files):
            print("Warning: Selection has changed since conversation started.")
            print("Use 'reset' to start a new conversation with the current selection.")
    
    def do_exit(self, arg):
        """Exit the REPL"""
        print("Goodbye!")
        return True
    
    # Alias for exit
    do_quit = do_exit

def main():
    parser = argparse.ArgumentParser(description="Claude Code REPL")
    parser.add_argument("--api-key", help="Anthropic API key")
    args = parser.parse_args()
    
    api_key = args.api_key or os.environ.get("ANTHROPIC_API_KEY")
    if not api_key:
        print("Please provide an Anthropic API key via --api-key or set the ANTHROPIC_API_KEY environment variable")
        return
    
    repl = CodeAgentRepl(api_key)
    repl.cmdloop()
    
if __name__ == "__main__":
    main()