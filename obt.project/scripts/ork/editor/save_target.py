################################################################################
# save_target — family-correct Save-target defaults + the Save action plan for the
# dflow editor shell. PURE (os only): the family policy lives here so the shell wires
# it into the house file requester and the headless gate asserts it without a window.
#
# The bug this closes: a DSL-backed doc used to serialize an .orj straight into the CWD
# (the serializer cannot overwrite a source .py, so Save fell back to <name>.orj in the
# process dir — an untracked file landing in the repo root). Save now ALWAYS routes
# through the file requester (defaults below), NEVER an implicit cwd write.
################################################################################

import os

_ORJ_EXTS = (".orj", ".json")


def save_defaults(binding, source):
  """(default_dir, default_name) for a Save dialog on `binding` (its `source` = the string
  the shell opened, parallel to the binding). Family policy:

    DSL-backed doc (binding.dsl_source set — particles / hypermesh CODE): Save-As semantics —
      the ASSET'S directory + '<title>.orj' (the .py is CODE, never written in-place). A
      bare-name asset with no path component falls back to the user's HOME (never the cwd).
    .orj / .json-backed doc: its OWN path preselected (dir + basename) — overwrite is the
      confirm-by-default selection.

  A savable family always carries either dsl_source or an .orj/.json source; the final branch
  is defensive and still resolves against the source's directory, never the cwd."""
  title = (getattr(binding, "title", None) or "graph")
  dsl = getattr(binding, "dsl_source", None)
  if dsl:
    d = os.path.dirname(str(dsl))
    if not (d and os.path.isdir(d)):
      d = os.path.expanduser("~")            # bare-name DSL asset: a safe start dir, NEVER cwd
    return (d, f"{title}.orj")
  s = str(source)
  if s.lower().endswith(_ORJ_EXTS):
    ap = os.path.abspath(s)
    return (os.path.dirname(ap), os.path.basename(ap))
  ap = os.path.abspath(s)                     # defensive: the source's dir, never a bare cwd
  return (os.path.dirname(ap), f"{title}.orj")


def normalize_orj(path):
  """Ensure a reflection-JSON extension on a user-chosen path (the dialog filters .orj and
  presets '<name>.orj'; a bare typed filename gets '.orj', matching the house save idiom)."""
  s = str(path)
  return s if s.lower().endswith(_ORJ_EXTS) else s + ".orj"


def plan_save(binding, source, remembered):
  """Decide the Save action. `remembered` = this doc's last successful save path in the
  session (or None). Returns ('resave', path) when a remembered path exists (re-save WITHOUT
  the dialog), else ('dialog', default_dir, default_name) (open the file requester defaulted
  family-correctly). Save-As is the shell calling with remembered=None (always dialogs)."""
  if remembered:
    return ("resave", str(remembered))
  default_dir, default_name = save_defaults(binding, source)
  return ("dialog", default_dir, default_name)
