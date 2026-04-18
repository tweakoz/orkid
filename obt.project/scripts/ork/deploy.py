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

# Find DEPLOY_ROOT by searching upward for .staging/
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIR="$SCRIPT_DIR"
while [ "$DIR" != "/" ]; do
  DIR="$(dirname "$DIR")"
  if [ -d "$DIR/.staging" ]; then
    DEPLOY_ROOT="$DIR/.staging"
    break
  fi
done
if [ -z "$DEPLOY_ROOT" ]; then
  echo "ERROR: could not find .staging/ directory" >&2
  exit 1
fi
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
  """Generate launcher script for gui mode (with Terminal.app).

  Default gui mode keeps a Terminal window so debug log output is visible.
  Apps that want a pure windowed launch should set "terminal": False in
  their spec, which selects the Mach-O launcher path
  (_install_macho_launcher) in generate_app_bundle.
  """
  return _terminal_launcher(command_parts)


def _shell_quote(s):
  """Simple shell-safe quoting for command parts."""
  if s == "" or any(c in s for c in " \t\n'\"\\$`!#&|;(){}[]<>?*~"):
    return "'" + s.replace("'", "'\\''") + "'"
  return s


###############################################################################
# Mach-O launcher installation (windowed gui mode, no Terminal.app)
###############################################################################
#
# Why this exists: when CFBundleExecutable is a bash script, the kernel exec
# chain runs /usr/bin/env as the actual Mach-O image. macOS TCC then attributes
# the running process to com.apple.env, NOT to the bundle. Privacy grants
# (Full Disk Access, Files & Folders → Desktop) silently fail to bind, no
# first-launch prompt fires, and reads of sibling files in TCC-protected
# folders (~/Desktop, ~/Documents, ~/Downloads) are denied at the kernel
# sandbox layer with EPERM. The only fix is to make CFBundleExecutable a real
# signed Mach-O binary that the kernel can attribute to the bundle's identity.
#
# That binary is obt-app-launcher, built mac-only by ork.core/tools/CMakeLists.txt
# from ork.core/tools/obt_app_launcher.c. One byte-identical universal binary
# is dropped into every windowed-mode .app across every orkid-based project.
# Per-app configuration lives entirely in Contents/Resources/launch.args.

# Info.plist usage descriptions added to windowed bundles. Without these the
# TCC first-launch prompt either doesn't fire or fires with a generic
# Apple-supplied message.
_TCC_USAGE_KEYS = {
  "NSDesktopFolderUsageDescription":
    "{name} accesses test data and runtime files alongside the app.",
  "NSDocumentsFolderUsageDescription":
    "{name} accesses project files in Documents.",
  "NSDownloadsFolderUsageDescription":
    "{name} accesses downloaded test assets.",
}


def _install_macho_launcher(contents_dir, launcher_binary, command_parts, name, bundle_id):
  """Install obt-app-launcher into a windowed-mode .app bundle.

  Drops the precompiled launcher into Contents/MacOS/obt-app-launcher and
  writes Contents/Resources/launch.args with the per-app argv. The caller is
  responsible for setting CFBundleExecutable to "obt-app-launcher" in the
  Info.plist BEFORE calling this function, since the bundle is signed here
  and the signature seals the Info.plist.

  argv translation: this function mirrors the bash terminal launcher's
  contract for the `command` field of an app spec. The bash launcher template
  hardcodes `--command "<joined>"`, so we replicate that here:

      command_parts = ["uni.app.fd2testrunner.py"]
        → launch.args lines: ["--command", "uni.app.fd2testrunner.py"]
        → effective argv:    obt-launch-env --command uni.app.fd2testrunner.py

      command_parts = ["foo.py", "arg1", "arg2"]
        → launch.args lines: ["--command", "foo.py arg1 arg2"]
        → effective argv:    obt-launch-env --command "foo.py arg1 arg2"

  This keeps the spec format identical to the terminal-mode contract so the
  same DEPLOY_CONFIG entries can flip between terminal=True and terminal=False
  without rewriting their command field. Apps with terminal=True (or
  mode=terminal) use _terminal_launcher and are not affected by this code.

  Args:
    contents_dir:    .app/Contents/ directory
    launcher_binary: Path to the precompiled obt-app-launcher Mach-O binary
                     (typically <staging>/bin/obt-app-launcher)
    command_parts:   list[str] — same as the bash launcher's command_parts;
                     joined into a single shell-quoted string and passed as
                     the value of `--command`
    name:            App display name (used for header comments only)
    bundle_id:       CFBundleIdentifier — passed to codesign --identifier so
                     the signed bundle has a stable, deterministic code
                     identity that TCC grants can anchor to across re-deploys

  Returns:
    Absolute path to the bundle that was signed (the parent of contents_dir)
  """
  contents_dir = str(contents_dir)
  app_dir = os.path.dirname(contents_dir)
  macos_dir = os.path.join(contents_dir, "MacOS")
  resources_dir = os.path.join(contents_dir, "Resources")
  os.makedirs(macos_dir, exist_ok=True)
  os.makedirs(resources_dir, exist_ok=True)

  # ---- Copy the launcher Mach-O ----
  if not launcher_binary or not os.path.isfile(launcher_binary):
    raise RuntimeError(
      f"obt-app-launcher binary not found at {launcher_binary}. "
      f"Build orkid (ork.core/tools/obt_app_launcher.c) before deploying.")

  dst_launcher = os.path.join(macos_dir, "obt-app-launcher")
  shutil.copy2(launcher_binary, dst_launcher)
  os.chmod(dst_launcher, 0o755)

  # ---- Translate command_parts to (--command, joined-cmd) ----
  # Mirrors _terminal_launcher's behavior exactly. Empty command_parts is
  # legal in terminal mode (interactive shell) but doesn't make sense in
  # windowed mode — there's no shell to be interactive in.
  if not command_parts:
    raise RuntimeError(
      f"windowed-mode app '{name}' has empty command — "
      f"a windowed bundle must specify a command to run "
      f"(or set terminal=True for an interactive shell)")
  cmd_str = " ".join(_shell_quote(c) for c in command_parts)
  launch_argv_lines = ["--command", cmd_str]

  # ---- Write launch.args ----
  args_path = os.path.join(resources_dir, "launch.args")
  with open(args_path, 'w') as f:
    f.write(f"# launch.args for {name}\n")
    f.write("# Each non-comment, non-blank line is one argv element passed to\n")
    f.write("# obt-launch-env. obt-app-launcher prepends $DEPLOY_ROOT/obt-launch-env\n")
    f.write("# as argv[0] automatically.\n")
    for line in launch_argv_lines:
      f.write(line + "\n")

  # ---- Sign the bundle ----
  # NEVER use --deep here. --deep would re-stamp the launcher binary inside
  # the bundle and change its cdhash on every deploy, breaking TCC grant
  # persistence (the user would have to re-grant Full Disk Access every time).
  # We pre-sign the launcher once at OBT build time and only stamp the
  # surrounding bundle (Info.plist + Resources) per-deploy.
  subprocess.run(
    ["codesign", "--force", "--sign", "-",
     "--identifier", bundle_id,
     "--options", "runtime",
     app_dir],
    check=True, capture_output=True)

  return app_dir


###############################################################################
# App bundle generation
###############################################################################

def generate_app_bundle(deploy_root, apps_dir, app_spec):
  """Create a .app bundle.

  Three launcher strategies are supported:

  - mode="terminal":               bash script that opens Terminal.app
  - mode="gui",  terminal=True:    same as terminal mode (debug-friendly default)
  - mode="gui",  terminal=False:   precompiled obt-app-launcher Mach-O binary
                                   as CFBundleExecutable, with NSDesktopFolderUsageDescription
                                   etc. so TCC anchors to the bundle's signed
                                   identity. No Terminal window is shown.

  Args:
    deploy_root: Path to the deployment root directory
    apps_dir:    Path to the apps/ directory within the deployment
    app_spec:    Dict with keys:
                   name: str          — bundle display name
                   command: list[str] — argv passed to obt-launch-env
                   mode: "terminal" | "gui"
                   terminal: bool     — gui-mode opt-out of Terminal.app
                                        (default True)
                   bundle_id: str     — optional, defaults to com.tweakoz.obt.{name}
                   icon: str          — optional path to .icns
                   launcher_binary: str — required when mode=gui & terminal=False;
                                          path to the precompiled obt-app-launcher

  Returns:
    Path to the created .app bundle
  """
  deploy_root = str(deploy_root)
  apps_dir = str(apps_dir)

  name = app_spec["name"]
  command = app_spec["command"]
  mode = app_spec.get("mode", "gui")
  use_terminal = app_spec.get("terminal", True)
  bundle_id = app_spec.get("bundle_id", f"com.tweakoz.obt.{name.lower()}")
  icon_path = app_spec.get("icon")
  launcher_binary = app_spec.get("launcher_binary")

  windowed = (mode == "gui" and not use_terminal)

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

  # ---- Info.plist ----
  # CFBundleExecutable depends on which launcher strategy we're using.
  exe_name = "obt-app-launcher" if windowed else "launch"
  plist = generate_info_plist(name, bundle_id, icon_filename, exe_name)

  # Windowed bundles need NS*UsageDescription keys so TCC fires a real
  # first-launch prompt attributed to the bundle.
  if windowed:
    for key, template in _TCC_USAGE_KEYS.items():
      plist[key] = template.format(name=name)

  plist_path = os.path.join(contents_dir, "Info.plist")
  with open(plist_path, 'wb') as f:
    plistlib.dump(plist, f)

  # ---- Launcher ----
  if windowed:
    # Mach-O launcher path: copy the prebuilt binary, write launch.args, sign.
    _install_macho_launcher(contents_dir, launcher_binary, command, name, bundle_id)
  else:
    # Bash launcher path: terminal mode (or gui mode with terminal=True).
    if mode == "terminal":
      script_content = _terminal_launcher(command)
    else:
      script_content = _gui_launcher(command)
    launch_path = os.path.join(macos_dir, "launch")
    with open(launch_path, 'w') as f:
      f.write(script_content)
    os.chmod(launch_path, 0o755)

  return app_dir
