"""Shared library for macOS .app bundle generation.

Reusable by any OBT project — projects declare app bundles in their deploy
manifest and this module generates the thin .app wrappers at the top level
of the visible deploy directory (alongside the hidden .staging/ infrastructure).
"""

import os
import plistlib
import shutil
import subprocess
import tempfile


###############################################################################
# Info.plist generation
###############################################################################

def generate_info_plist(name, bundle_id, icon_filename, executable_name):
  """Return an Info.plist dict suitable for a macOS .app bundle.

  Args:
    name:            Bundle display name (e.g. "OrkTestRunner")
    bundle_id:       CFBundleIdentifier (e.g. "com.tweakoz.orkid.testrunner")
    icon_filename:   .icns filename in Resources/ (e.g. "orkid.icns"), or None
    executable_name: Name of the executable in MacOS/ (e.g. "launch")

  Returns:
    dict ready for plistlib.dump()
  """
  plist = {
    "CFBundleName": name,
    "CFBundleDisplayName": name,
    "CFBundleIdentifier": bundle_id,
    "CFBundleVersion": "1.0",
    "CFBundleShortVersionString": "1.0",
    "CFBundlePackageType": "APPL",
    "CFBundleExecutable": executable_name,
    "CFBundleInfoDictionaryVersion": "6.0",
    "LSMinimumSystemVersion": "13.0",
    "NSHighResolutionCapable": True,
  }
  if icon_filename:
    plist["CFBundleIconFile"] = icon_filename
  return plist


###############################################################################
# Icon conversion
###############################################################################

# Standard icon sizes for macOS .iconset
_ICON_SIZES = [
  (16,   "icon_16x16.png"),
  (32,   "icon_16x16@2x.png"),
  (32,   "icon_32x32.png"),
  (64,   "icon_32x32@2x.png"),
  (128,  "icon_128x128.png"),
  (256,  "icon_128x128@2x.png"),
  (256,  "icon_256x256.png"),
  (512,  "icon_256x256@2x.png"),
  (512,  "icon_512x512.png"),
  (1024, "icon_512x512@2x.png"),
]


def png_to_icns(png_path, icns_path):
  """Convert a PNG image to macOS .icns using sips + iconutil.

  Non-square images are padded (not stretched) onto a transparent square
  canvas so the aspect ratio is preserved.

  Args:
    png_path:  Path to source PNG
    icns_path: Destination path for the .icns file

  Raises:
    subprocess.CalledProcessError on tool failure
  """
  png_path = str(png_path)
  icns_path = str(icns_path)

  with tempfile.TemporaryDirectory() as tmpdir:
    iconset_dir = os.path.join(tmpdir, "icon.iconset")
    os.makedirs(iconset_dir)

    # Create a square padded source at 1024x1024
    square_src = os.path.join(tmpdir, "square.png")
    # Resample longest edge to 1024, then pad to 1024x1024
    info = subprocess.run(
      ["sips", "-g", "pixelWidth", "-g", "pixelHeight", png_path],
      capture_output=True, text=True, check=True)
    w = h = 0
    for line in info.stdout.splitlines():
      if "pixelWidth" in line:
        w = int(line.split()[-1])
      if "pixelHeight" in line:
        h = int(line.split()[-1])
    # Scale so the larger dimension fits 1024, preserving aspect ratio
    if w >= h:
      new_w, new_h = 1024, max(1, int(1024 * h / w))
    else:
      new_w, new_h = max(1, int(1024 * w / h)), 1024
    subprocess.run(
      ["sips", "-z", str(new_h), str(new_w), png_path,
       "--out", square_src],
      check=True, capture_output=True)
    # Pad to 1024x1024 square (centers the image on transparent canvas)
    subprocess.run(
      ["sips", "-p", "1024", "1024", square_src],
      check=True, capture_output=True)

    for size, filename in _ICON_SIZES:
      out_path = os.path.join(iconset_dir, filename)
      subprocess.run(
        ["sips", "-z", str(size), str(size), square_src,
         "--out", out_path],
        check=True, capture_output=True)

    subprocess.run(
      ["iconutil", "-c", "icns", iconset_dir, "-o", icns_path],
      check=True, capture_output=True)


###############################################################################
# Folder icon
###############################################################################

def set_folder_icon(folder_path, image_path):
  """Set a custom Finder icon on a macOS folder.

  Uses osascript with Cocoa ObjC bridge — zero external dependencies.
  Accepts PNG, ICNS, or any macOS-recognized image format.
  """
  folder_path = str(folder_path)
  image_path = os.path.abspath(str(image_path))
  applescript = (
    'use framework "Cocoa"\n'
    'set sourcePath to "%s"\n'
    'set destPath to "%s"\n'
    'set imageData to (current application\'s NSImage\'s alloc()\'s '
    'initWithContentsOfFile:sourcePath)\n'
    '(current application\'s NSWorkspace\'s sharedWorkspace()\'s '
    'setIcon:imageData forFile:destPath options:2)\n'
  ) % (image_path, folder_path)
  result = subprocess.run(
    ['osascript', '-e', applescript],
    capture_output=True, text=True)
  if result.returncode != 0:
    raise RuntimeError(f"Failed to set folder icon: {result.stderr.strip()}")


###############################################################################
# Launcher script templates
###############################################################################

# All launchers delegate to obt-launch-env which already handles venv
# bootstrap, relocation fixup, project discovery, and obt.env.launch.py
# invocation. The .app just needs to find DEPLOY_ROOT and call it.

_LAUNCHER_HEADER = r'''#!/usr/bin/env bash
###############################################################################
# Auto-generated by ork.deploy — thin .app launcher
###############################################################################

# Compute DEPLOY_ROOT: Foo.app/Contents/MacOS/launch → ../../../.staging
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEPLOY_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)/.staging"
'''


def _terminal_launcher(command_parts):
  """Generate launcher script for terminal mode (opens Terminal.app).

  Delegates to obt-launch-env which already handles all env setup,
  project discovery, and obt.env.launch.py invocation (with "$@" passthrough).
  """
  if command_parts:
    cmd_str = " ".join(_shell_quote(c) for c in command_parts)
    return _LAUNCHER_HEADER + r'''
# Terminal mode: open Terminal.app running a command in OBT env
TMPSCRIPT="$(mktemp /tmp/obt-launch-XXXXXX.sh)"
cat > "$TMPSCRIPT" << EOF
#!/usr/bin/env bash
rm -f "\$0"
exec "$DEPLOY_ROOT/obt-launch-env" --command "%s"
EOF
chmod +x "$TMPSCRIPT"
open -a Terminal.app "$TMPSCRIPT"
''' % cmd_str
  else:
    return _LAUNCHER_HEADER + r'''
# Terminal mode: open Terminal.app with OBT interactive shell
open -a Terminal.app "$DEPLOY_ROOT/obt-launch-env"
'''


def _gui_launcher(command_parts):
  """Generate launcher script for gui mode (runs command directly).

  Uses obt-launch-env --command to run the gui app within the fully
  configured OBT environment.
  """
  cmd_str = " ".join(_shell_quote(c) for c in command_parts)
  return _LAUNCHER_HEADER + r'''
# GUI mode: launch command inside OBT environment
exec "$DEPLOY_ROOT/obt-launch-env" --command "%s"
''' % cmd_str


def _shell_quote(s):
  """Simple shell-safe quoting for command parts."""
  if s == "" or any(c in s for c in " \t\n'\"\\$`!#&|;(){}[]<>?*~"):
    return "'" + s.replace("'", "'\\''") + "'"
  return s


###############################################################################
# App bundle generation
###############################################################################

def generate_app_bundle(deploy_root, apps_dir, app_spec):
  """Create a thin .app bundle.

  Args:
    deploy_root: Path to the deployment root directory
    apps_dir:    Path to the apps/ directory within the deployment
    app_spec:    Dict with keys:
                   name: str          — bundle display name
                   command: list[str] — command to run
                   mode: "terminal" | "gui"
                   bundle_id: str     — optional, defaults to com.tweakoz.obt.{name}
                   icon: str          — optional path to .icns file in apps_dir

  Returns:
    Path to the created .app bundle
  """
  deploy_root = str(deploy_root)
  apps_dir = str(apps_dir)

  name = app_spec["name"]
  command = app_spec["command"]
  mode = app_spec.get("mode", "gui")
  bundle_id = app_spec.get("bundle_id", f"com.tweakoz.obt.{name.lower()}")
  icon_path = app_spec.get("icon")  # path to .icns in apps_dir

  app_dir = os.path.join(apps_dir, f"{name}.app")
  contents_dir = os.path.join(app_dir, "Contents")
  macos_dir = os.path.join(contents_dir, "MacOS")
  resources_dir = os.path.join(contents_dir, "Resources")

  os.makedirs(macos_dir, exist_ok=True)
  os.makedirs(resources_dir, exist_ok=True)

  # Determine icon filename for Info.plist
  icon_filename = None
  if icon_path and os.path.isfile(icon_path):
    icon_filename = os.path.basename(icon_path)
    dst_icon = os.path.join(resources_dir, icon_filename)
    shutil.copy2(icon_path, dst_icon)

  # Write Info.plist
  plist = generate_info_plist(name, bundle_id, icon_filename, "launch")
  plist_path = os.path.join(contents_dir, "Info.plist")
  with open(plist_path, 'wb') as f:
    plistlib.dump(plist, f)

  # Generate launcher script
  if mode == "terminal":
    script_content = _terminal_launcher(command)
  else:
    script_content = _gui_launcher(command)

  launch_path = os.path.join(macos_dir, "launch")
  with open(launch_path, 'w') as f:
    f.write(script_content)
  os.chmod(launch_path, 0o755)

  return app_dir
