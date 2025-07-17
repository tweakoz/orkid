#!/usr/bin/env python3

"""
NotCurses cleanup utility for Python scripts.

This module ensures that NotCurses terminal state is properly restored
even when Python scripts are interrupted with Ctrl-C (SIGINT).

Usage:
    import ork.core.pyfiles.ncui_cleanup as ncui_cleanup
    ncui_cleanup.setup()

Or use as context manager:
    with ncui_cleanup.ensure_cleanup():
        # your code here
        pass
"""

import signal
import sys
import os
import atexit
import gc
import weakref
from typing import Optional, Callable, Any
from contextlib import contextmanager

_cleanup_installed = False
_original_sigint_handler = None
_original_sigterm_handler = None
_cleanup_callbacks = []
_context_refs = []

def reset_terminal_state():
    """Reset terminal input modes that NotCurses might have set"""
    try:
        # Send escape sequences to reset various input modes
        reset_sequences = [
            "\033[?1l",        # Disable application cursor keys
            "\033[>4;0m",      # Reset modifyOtherKeys mode
            "\033[?2004l",     # Disable bracketed paste
            "\033[?1006l",     # Disable SGR mouse mode
            "\033[?1015l",     # Disable urxvt mouse mode
            "\033[?1003l",     # Disable mouse tracking
            "\033[?1002l",     # Disable cell motion mouse tracking
            "\033[?1000l",     # Disable X11 mouse tracking
            "\033[?47l",       # Restore normal screen buffer
            "\033[?1049l",     # Disable alternate screen buffer
        ]
        
        for seq in reset_sequences:
            os.write(sys.stdout.fileno(), seq.encode())
        
        # Force flush
        os.fsync(sys.stdout.fileno())
        
    except Exception as e:
        # In case of any error, write to stderr
        try:
            sys.stderr.write(f"Warning: Failed to reset terminal state: {e}\n")
            sys.stderr.flush()
        except:
            pass

def force_cleanup():
    """Force cleanup of NotCurses contexts"""
    try:
        # Force garbage collection to trigger C++ destructors
        gc.collect()
        
        # Call any registered cleanup callbacks
        for callback in _cleanup_callbacks:
            try:
                callback()
            except Exception as e:
                try:
                    sys.stderr.write(f"Warning: Cleanup callback failed: {e}\n")
                    sys.stderr.flush()
                except:
                    pass
        
        # Try to shutdown any remaining contexts
        for context_ref in _context_refs:
            try:
                context = context_ref()
                if context is not None:
                    context.shutdown()
            except Exception:
                pass
        
        # Reset terminal state
        reset_terminal_state()
        
    except Exception as e:
        # Last resort - write to stderr
        try:
            sys.stderr.write(f"Warning: Force cleanup failed: {e}\n")
            sys.stderr.flush()
        except:
            pass

def signal_handler(signum, frame):
    """Handle SIGINT and SIGTERM by performing cleanup"""
    force_cleanup()
    
    # Call original handler if it exists
    if signum == signal.SIGINT and _original_sigint_handler:
        if callable(_original_sigint_handler):
            _original_sigint_handler(signum, frame)
        else:
            signal.signal(signal.SIGINT, _original_sigint_handler)
            os.kill(os.getpid(), signal.SIGINT)
    elif signum == signal.SIGTERM and _original_sigterm_handler:
        if callable(_original_sigterm_handler):
            _original_sigterm_handler(signum, frame)
        else:
            signal.signal(signal.SIGTERM, _original_sigterm_handler)
            os.kill(os.getpid(), signal.SIGTERM)
    else:
        # Default behavior - exit
        sys.exit(128 + signum)

def setup():
    """Install signal handlers and cleanup mechanisms"""
    global _cleanup_installed, _original_sigint_handler, _original_sigterm_handler
    
    if _cleanup_installed:
        return
    
    _cleanup_installed = True
    
    # Install signal handlers
    _original_sigint_handler = signal.signal(signal.SIGINT, signal_handler)
    _original_sigterm_handler = signal.signal(signal.SIGTERM, signal_handler)
    
    # Install atexit handler
    atexit.register(force_cleanup)

def register_cleanup_callback(callback: Callable[[], None]):
    """Register a cleanup callback to be called during shutdown"""
    _cleanup_callbacks.append(callback)

def register_context(context: Any):
    """Register a NotCurses context for cleanup tracking"""
    _context_refs.append(weakref.ref(context))

def is_setup() -> bool:
    """Check if cleanup is already set up"""
    return _cleanup_installed

@contextmanager
def ensure_cleanup():
    """Context manager that ensures cleanup happens"""
    setup()
    try:
        yield
    finally:
        force_cleanup()

# Auto-setup when module is imported
# This ensures cleanup happens even if user forgets to call setup()
try:
    setup()
except Exception:
    # If setup fails, don't prevent import
    pass 