################################################################################
# ork/ui/app_state.py — per-user, per-app persisted UI state (dock layouts, ...).
#
# The persisted-state slot is keyed by the EZAPP IDENTITY: the app's declared
# name + the realpath of the top-level entry script (sys.argv[0]). This gives:
#   - deterministic for a given (name, path),
#   - distinct editors/tools get distinct slots with zero per-editor naming,
#   - ANY future ezapp using DockSpace inherits persistence for free.
#
#   <user-state-root>/orkid/appstate/<appname>-<shorthash(entrypath)>/dock_layout.json
#
# The user-state root is obt.path.user_global() (~/.obt-global) — user-local,
# OUTSIDE the repo. Content stays machine-scrub-clean; the entry-path hash lives
# only in the (user-local) directory name.
#
# NOTE: relocating the source tree changes the entry-path hash, hence the key —
# persisted state resets. That is deliberate: identity is keyed to (name, path).
################################################################################

import hashlib
import os
import sys

import obt.path

################################################################################

def _short_hash(text, n=12):
  return hashlib.sha256(text.encode("utf-8")).hexdigest()[:n]

def _entry_path(entry_path=None):
  # the top-level source file that launched this process; realpath so symlinked
  # launchers resolve to the same slot as their target.
  raw = entry_path if entry_path is not None else (sys.argv[0] or "")
  return os.path.realpath(raw) if raw else "<unknown>"

################################################################################

def state_slot(app_name, entry_path=None):
  """The (name, entry-path) identity key for this app's persisted-state slot."""
  return f"{app_name}-{_short_hash(_entry_path(entry_path))}"

def session_state_dir(app_name, entry_path=None, create=True):
  """The per-user, per-app state directory (created on demand)."""
  root = obt.path.user_global() / "orkid" / "appstate" / state_slot(app_name, entry_path)
  if create:
    os.makedirs(str(root), exist_ok=True)
  return str(root)

def dock_layout_path(app_name, entry_path=None, create=True):
  """Absolute path to this app's persisted DockSpace layout JSON."""
  return os.path.join(session_state_dir(app_name, entry_path, create=create), "dock_layout.json")

################################################################################

def text_input_has_focus(uicontext):
  """True when a text-input WIDGET owns keyboard focus. Editor-app-level layout
  chords (e.g. Shift+L 'reset layout') suppress themselves in this state so the key
  reaches the focused CodeView / F32Edit / IntEdit / OverlayLineEdit instead of
  mutating the layout.

  The signal is keyboard_focus_widget (set by editable widgets when they take the
  caret). NOTE: uicontext.hasKeyboardFocus is deliberately NOT consulted — that is
  the WINDOW's OS keyboard-focus bool (GOT/LOST_KEYFOCUS), true whenever the window
  is focused, which would suppress the chord everywhere."""
  if uicontext is None:
    return False
  try:
    return uicontext.keyboard_focus_widget is not None
  except Exception:
    return False
