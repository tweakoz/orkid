#!/usr/bin/env python3
"""
macOS app bundle deployment module for creating in-place launchers.
This module provides functionality to create .app bundles that launch
development executables with captured environment variables.
"""

import sys, os, json, re, shutil, plistlib, datetime, subprocess
from obt import path, pathtools, command, deco
from pathlib import Path

deco = deco.Deco()

class BundleCreator:
    """Creates macOS app bundles with captured environment for development builds."""
    
    def __init__(self):
        self.orkid_src_dir = path.orkid()
        self.temp_dir = path.temp() / "inplace_bundles"
        
    def create_bundle(self, executable_name, exec_args, bundle_name, 
                     additional_env=None, icon_path=None, additional_capture_vars=None):
        """
        Create a macOS app bundle.
        
        Args:
            executable_name: Name of the executable (without path)
            exec_args: Arguments for the executable
            bundle_name: Bundle name for the app (CFBundleName)
            additional_env: Dict of additional environment variables to include
            icon_path: Optional custom icon path (defaults to orkidlogo.icns)
            additional_capture_vars: List of additional env var names to capture
        
        Returns:
            Path to created bundle on Desktop
        """
        # Add orkid's bin directory to sys.path to find _debug_helpers
        import sys
        orkid_bin_dir = self.orkid_src_dir / "obt.project" / "bin"
        if str(orkid_bin_dir) not in sys.path:
            sys.path.insert(0, str(orkid_bin_dir))
        
        # Import _debug_helpers
        import _debug_helpers
        
        # Create args object for compatibility
        class Args:
            def __init__(self, executable_name, exec_args, bundlename):
                self.executable_name = executable_name
                self.exec_args = exec_args
                self.bundlename = bundlename
        
        args = Args(executable_name, exec_args, bundle_name)
        
        # Get executable path and arguments
        exe_path, exe_args, exe_name = _debug_helpers.get_exec_and_args(args)
        exe_base = os.path.basename(exe_path)
        
        is_python_script = "python" in exe_base and ".py" in exe_args[0]
        
        print(deco.yellow("=== In-Place Deploy Configuration ==="))
        print(f"exe_path: {exe_path}")
        print(f"exe_base: {exe_base}")
        print(f"exe_args: {exe_args}")
        print(f"exe_name: {exe_name}")
        print(f"is_python_script: {is_python_script}")
        print(f"bundle_name: {bundle_name}")
        
        # Define directories
        bundle_dir = self.temp_dir / f"{bundle_name}.app" / "Contents"
        
        # Determine icon path
        if icon_path is None:
            src_icon = self.orkid_src_dir/"ork.tool"/"OrkidTool.app"/"Contents"/"Resources"/"orkidlogo.icns"
        else:
            src_icon = Path(icon_path)
            
        dst_icon = bundle_dir / "Resources" / "orkidlogo.icns"
        dst_launcher_script = bundle_dir / 'MacOS' / 'launcher.sh'
        dst_launcher_binary = bundle_dir / 'MacOS' / 'launcher'
        
        # Capture environment
        captured_env = self._capture_environment(additional_env, additional_capture_vars)
        
        # Create launch configuration
        launch_config = self._create_launch_config(exe_path, exe_name, exe_args, 
                                                   is_python_script)
        
        # Create bundle structure
        self._create_bundle_structure(bundle_dir, src_icon, dst_icon)
        
        # Save configurations
        self._save_configurations(bundle_dir, captured_env, launch_config)
        
        # Generate and write launcher script
        launcher_script = self._generate_launcher_script()
        with open(dst_launcher_script, 'w') as f:
            f.write(launcher_script)
        dst_launcher_script.chmod(0o755)
        
        # Compile launcher wrapper
        self._compile_launcher_wrapper(dst_launcher_binary)
        
        # Create Info.plist
        self._create_info_plist(bundle_dir, bundle_name)
        
        # Copy to Desktop and sign
        desktop_path = self._finalize_bundle(bundle_name)
        
        return desktop_path
    
    def _capture_environment(self, additional_env=None, additional_capture_vars=None):
        """Capture current environment variables."""
        print(deco.yellow("\n=== Capturing Environment ==="))
        
        # Base environment variables to capture
        important_vars = [
            # Core paths
            "PATH", "PYTHONPATH", "PYTHONHOME", "VIRTUAL_ENV",
            "LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH", "DYLD_FALLBACK_LIBRARY_PATH",
            
            # OBT variables
            "OBT_STAGE", "OBT_ORIGINAL_PATH", "OBT_ORIGINAL_PYTHONPATH",
            "OBT_PYTHON_BINDIR", "OBT_PYTHON_LIBDIR",
            
            # Orkid variables
            "ORKID_WORKSPACE_DIR", "ORKID_LEV2_EXAMPLES_DIR", "ORKID_CORE_EXAMPLES_DIR",
            
            # Build tools
            "CC", "CXX", "CMAKE_PREFIX_PATH",
            
            # Graphics/Audio
            "MESA_GL_VERSION_OVERRIDE", "MESA_GLSL_VERSION_OVERRIDE", "AUDIODEV",
        ]
        
        # Add any additional variables to capture
        if additional_capture_vars:
            important_vars.extend(additional_capture_vars)
        
        captured_env = {}
        
        # Capture OBT_ and ORKID_ prefixed variables
        for var in os.environ:
            if var.startswith(("OBT_", "ORKID_")) or var in important_vars:
                value = os.environ.get(var)
                if value:
                    captured_env[var] = value
                    print(f"  {var}: {value[:60]}{'...' if len(value) > 60 else ''}")
        
        # Add any additional environment variables
        if additional_env:
            for key, value in additional_env.items():
                if value is not None:
                    captured_env[key] = value
                    print(f"  {key}: {value[:60]}{'...' if len(value) > 60 else ''}")
        
        return captured_env
    
    def _create_launch_config(self, exe_path, exe_name, exe_args, is_python_script):
        """Create launch configuration."""
        launch_config = {
            "executable_path": str(exe_path),
            "executable_name": exe_name,
            "exec_args": exe_args,
            "is_python_script": is_python_script,
            "working_directory": os.getcwd(),
            "deployment_time": datetime.datetime.now().isoformat(),
        }
        
        # Build the full command
        if is_python_script:
            # Use ork.python from the staging environment for Python scripts
            ork_python = os.path.join(os.environ.get('OBT_STAGE', ''), 'bin', 'ork.python')
            script_path = exe_args[0]
            remaining_args = exe_args[1:] if len(exe_args) > 1 else []
            # Quote arguments that contain spaces
            quoted_args = [f'"{arg}"' if ' ' in arg else arg for arg in remaining_args]
            launch_config["full_command"] = f'{ork_python} {script_path} {" ".join(quoted_args)}'
        else:
            # Quote arguments that contain spaces
            quoted_args = [f'"{arg}"' if ' ' in arg else arg for arg in exe_args]
            launch_config["full_command"] = f'{exe_path} {" ".join(quoted_args)}'
        
        print(deco.yellow("\n=== Launch Configuration ==="))
        print(f"Working Directory: {launch_config['working_directory']}")
        print(f"Full Command: {launch_config['full_command']}")
        
        return launch_config
    
    def _create_bundle_structure(self, bundle_dir, src_icon, dst_icon):
        """Create the macOS bundle directory structure."""
        print(deco.yellow("\n=== Creating Bundle Structure ==="))
        
        self._mkdir(self.temp_dir, wipe=False)
        self._mkdir(bundle_dir / "MacOS", wipe=True)
        self._mkdir(bundle_dir / "Resources", wipe=True)
        
        # Copy icon
        shutil.copy(str(src_icon), str(dst_icon))
        print(f"Copied icon to: {dst_icon}")
    
    def _save_configurations(self, bundle_dir, captured_env, launch_config):
        """Save environment and launch configurations."""
        env_config = {
            "captured_env": captured_env,
            "deployment_info": {
                "created": datetime.datetime.now().isoformat(),
                "source_host": os.uname().nodename,
                "cwd": os.getcwd(),
            }
        }
        
        # Write JSON files
        with open(bundle_dir / "Resources" / "environment.json", 'w') as f:
            json.dump(env_config, f, indent=2)
        
        with open(bundle_dir / "Resources" / "launch_config.json", 'w') as f:
            json.dump(launch_config, f, indent=2)
    
    def _generate_launcher_script(self):
        """Generate the launcher shell script."""
        return '''#!/usr/bin/env bash

# Get the bundle path
BUNDLE_DIR="$(cd "$(dirname "$0")/.." && pwd)"
RESOURCES_DIR="$BUNDLE_DIR/Resources"

# Create a self-contained launch command
# We'll pass everything as a single bash command to Terminal
TEMP_FILE="/tmp/orkid_env_$$.sh"

# Create debug log
DEBUG_LOG="/tmp/orkid_debug_$$.log"
echo "=== Orkid Launcher Debug Log ===" > "$DEBUG_LOG"
echo "Date: $(date)" >> "$DEBUG_LOG"
echo "BUNDLE_DIR: $BUNDLE_DIR" >> "$DEBUG_LOG"
echo "RESOURCES_DIR: $RESOURCES_DIR" >> "$DEBUG_LOG"
echo "TEMP_FILE: $TEMP_FILE" >> "$DEBUG_LOG"
echo "" >> "$DEBUG_LOG"

# Generate the complete environment setup script using Python
# Try to find python3 in common locations
PYTHON3=""
for py in "/opt/homebrew/bin/python3" "/usr/local/bin/python3" "/usr/bin/python3"; do
    if [ -x "$py" ]; then
        PYTHON3="$py"
        break
    fi
done

if [ -z "$PYTHON3" ]; then
    echo "ERROR: Could not find python3!" >> "$DEBUG_LOG"
    exit 1
fi

echo "Using Python: $PYTHON3" >> "$DEBUG_LOG"
echo "Python version: $($PYTHON3 --version 2>&1)" >> "$DEBUG_LOG"

BUNDLE_DIR="$BUNDLE_DIR" "$PYTHON3" - <<'PYTHON_END' > "$TEMP_FILE" 2>> "$DEBUG_LOG"
import json
import os
import sys
import shlex

# Get bundle directory from command line argument
bundle_dir = os.environ.get('BUNDLE_DIR', '')
resources_dir = f"{bundle_dir}/Resources"

# Load configurations
try:
    with open(f"{resources_dir}/environment.json") as f:
        env_config = json.load(f)
    with open(f"{resources_dir}/launch_config.json") as f:
        launch_config = json.load(f)
except Exception as e:
    print(f"echo 'Error loading config: {e}'")
    sys.exit(1)

# Start with shebang and header
print("#!/bin/bash")
print("set -e  # Exit on error")
print("")
print("echo '================================'")
print("echo 'Orkid Development Environment'")
print("echo '================================'")
print("echo ''")

# First, handle OBT_STAGE specially to ensure it's available
if "OBT_STAGE" in env_config["captured_env"]:
    obt_stage = env_config["captured_env"]["OBT_STAGE"]
    print(f"echo 'Setting OBT_STAGE to: {obt_stage}'")
    print(f"export OBT_STAGE='{obt_stage}'")
    print("")
    
    # Set PATH with staging directories FIRST
    print("echo 'Setting PATH with staging directories...'")
    prev_path = env_config["captured_env"]["PATH"]
    print(f"export PATH={prev_path}")
    #'{obt_stage}/bin:{obt_stage}/pyvenv/bin:/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin'")
    print("echo 'PATH set to:' $PATH")
    print("")
else:
    print("echo 'ERROR: OBT_STAGE not found in environment!'")
    print("")

# Export all other environment variables
print(f"echo 'Exporting {len(env_config['captured_env'])} environment variables...'")
print("# Export captured environment variables")
exported_count = 0
for key, value in env_config["captured_env"].items():
    if key != "PATH":  # We already set PATH above
        # Use shlex.quote for proper shell escaping
        print(f'export {key}={shlex.quote(value)}')
        exported_count += 1
print(f"echo 'Exported {exported_count} additional environment variables'")

# Change to working directory
working_dir = launch_config["working_directory"]
print(f"")
print(f"# Change to working directory")
print(f"cd '{working_dir}'")
print(f"")

# Store launch info in environment
full_command = launch_config["full_command"]
print(f"export ORKID_LAUNCH_COMMAND='{full_command}'")
print(f"export ORKID_ENV_SCRIPT='$0'")
print("")

# Display environment info
print("echo 'Environment configured:'")
print("echo '  Working directory:' $(pwd)")
print("echo '  OBT_STAGE:' $OBT_STAGE")
print("echo '  PATH:' $PATH")
print("echo '  Environment script:' $ORKID_ENV_SCRIPT")
print("echo '  Command to run:' $ORKID_LAUNCH_COMMAND")
print("echo ''")
print("echo 'You are now in a bash shell with the full Orkid environment.'")
print(r'echo "To run the original command, type: eval \$ORKID_LAUNCH_COMMAND"')
print("echo ''")

# Use exec to replace the current shell with bash and run the command
print("# Execute the command directly")
print('exec /bin/bash --norc -c "$ORKID_LAUNCH_COMMAND"')
PYTHON_END

# Check if Python succeeded
if [ $? -ne 0 ]; then
    echo "ERROR: Python script failed to generate environment!" >> "$DEBUG_LOG"
    exit 1
fi

# Debug: Check script content
echo "Generated script size: $(wc -c < "$TEMP_FILE") bytes" >> "$DEBUG_LOG"
echo "First 50 lines of generated script:" >> "$DEBUG_LOG"
echo "---" >> "$DEBUG_LOG"
head -50 "$TEMP_FILE" >> "$DEBUG_LOG"
echo "---" >> "$DEBUG_LOG"

# Make the script executable
chmod +x "$TEMP_FILE"

# Create the actual command that Terminal will execute
# Run the script directly which will set env and exec bash
TERMINAL_CMD="'$TEMP_FILE'"

echo "Terminal command: $TERMINAL_CMD" >> "$DEBUG_LOG"

# Launch Terminal with the command using open
open -a Terminal "$TEMP_FILE"

# Check if osascript succeeded
if [ $? -eq 0 ]; then
    echo "Terminal launch succeeded" >> "$DEBUG_LOG"
else
    echo "Terminal launch failed!" >> "$DEBUG_LOG"
fi

# Final debug info
echo "" >> "$DEBUG_LOG"
echo "Debug log: $DEBUG_LOG" >> "$DEBUG_LOG"
echo "Environment script: $TEMP_FILE" >> "$DEBUG_LOG"

# Also output to stderr so deployment script shows it
echo "Debug log created at: $DEBUG_LOG" >&2
echo "Environment script at: $TEMP_FILE" >&2

exit 0
'''
    
    def _compile_launcher_wrapper(self, dst_launcher_binary):
        """Compile the launcher wrapper."""
        print(deco.yellow("\n=== Compiling Launcher Wrapper ==="))
        wrapper_source = self.orkid_src_dir / "ork.data" / "misc" / "osx_launcher_wrapper.c"
        compile_cmd = [
            "cc", "-o", str(dst_launcher_binary),
            str(wrapper_source),
            "-framework", "Foundation"
        ]
        result = subprocess.run(compile_cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"Error compiling launcher: {result.stderr}")
            # Fall back to copying script as executable
            shutil.copy(str(dst_launcher_binary.parent / "launcher.sh"), str(dst_launcher_binary))
        else:
            print("Launcher wrapper compiled successfully")
        dst_launcher_binary.chmod(0o755)
    
    def _create_info_plist(self, bundle_dir, bundle_name):
        """Create Info.plist."""
        bundle_id = self._format_bundle_identifier(bundle_name)
        
        info_plist = {
            'CFBundleDisplayName': f"{bundle_name} (Dev)",
            'CFBundleExecutable': 'launcher',
            'CFBundleIdentifier': bundle_id,
            'CFBundleName': bundle_name,
            'CFBundleVersion': '1.0.0',
            'CFBundleIconFile': 'orkidlogo.icns',
            'CFBundlePackageType': 'APPL',
            'CFBundleSignature': '????',
            'LSMinimumSystemVersion': '10.13.0',
            'LSUIElement': False,  # Show in Dock
            'NSHighResolutionCapable': True,
            'NSSupportsAutomaticTermination': False,
        }
        
        with open(bundle_dir / "Info.plist", 'wb') as plist_file:
            plistlib.dump(info_plist, plist_file)
    
    def _finalize_bundle(self, bundle_name):
        """Copy bundle to Desktop and sign it."""
        bundle_path = self.temp_dir / f"{bundle_name}.app"
        desktop_path = Path.home() / "Desktop" / f"{bundle_name}.app"
        
        # Copy to Desktop
        print(deco.yellow("\n=== Copying Bundle to Desktop ==="))
        if desktop_path.exists():
            shutil.rmtree(desktop_path)
        shutil.copytree(bundle_path, desktop_path)
        
        # Ad-hoc sign the bundle
        print(deco.yellow("\n=== Signing Bundle ==="))
        sign_cmd = ["codesign", "--force", "--deep", "--sign", "-", str(desktop_path)]
        sign_result = subprocess.run(sign_cmd, capture_output=True, text=True)
        if sign_result.returncode == 0:
            print("Bundle signed successfully (ad-hoc)")
        else:
            print(f"Warning: Could not sign bundle: {sign_result.stderr}")
        
        print(deco.yellow("\n=== Bundle Created Successfully ==="))
        print(f"Bundle Location: {desktop_path}")
        print(f"Bundle Size: ~{sum(f.stat().st_size for f in desktop_path.rglob('*') if f.is_file()) / 1024:.1f} KB")
        print(deco.yellow("\nTo use:"))
        print(f"Double-click {bundle_name}.app on your Desktop to launch")
        print("\nNote: This bundle references your development environment.")
        print("It will only work on this machine with the current dev setup.")
        
        return desktop_path
    
    def _format_bundle_identifier(self, bundle_name):
        """Format bundle identifier."""
        # Sanitize the bundle_name to create a legal identifier
        sanitized_name = re.sub(r'[^a-zA-Z0-9.]+', '', bundle_name.replace(' ', ''))
        return f"com.tweakoz.orkid.dev.{sanitized_name}"
    
    def _mkdir(self, folder, wipe=False):
        """Helper to create directories."""
        if wipe and folder.exists():
            shutil.rmtree(str(folder))
        os.makedirs(folder, exist_ok=True)


# Convenience function for simple usage
def create_bundle(executable_name, exec_args, bundle_name, **kwargs):
    """
    Create a macOS app bundle.
    
    Args:
        executable_name: Name of the executable (without path)
        exec_args: Arguments for the executable
        bundle_name: Bundle name for the app (CFBundleName)
        **kwargs: Additional arguments passed to BundleCreator.create_bundle()
    
    Returns:
        Path to created bundle on Desktop
    """
    creator = BundleCreator()
    return creator.create_bundle(executable_name, exec_args, bundle_name, **kwargs)