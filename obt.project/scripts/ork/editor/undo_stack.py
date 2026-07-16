################################################################################
# Editor undo/redo stack — GENERIC (host-agnostic). The terrain editor hosts it
# today; ecsedit and the hypermesh editor can host the SAME module: state is an
# OPAQUE snapshot the host captures/restores (terrain: doc-JSON + DSL kwargs +
# display key), so nothing here knows about documents, scenes, or meshes.
#
# Pattern: the host applies its mutation through its normal code paths, then
# calls record(pre, post, restore, ...) with the snapshot taken BEFORE and AFTER.
# undo() hands the pre-snapshot back to the host's restore callable; redo() the
# post-snapshot. Coalescing: consecutive record()s with the SAME coalesce_key
# within coalesce_window seconds collapse into ONE step (drag ticks -> one undo),
# keeping the FIRST pre-state and the LATEST post-state.
################################################################################

import time


class SnapshotCommand:
  __slots__ = ("label", "coalesce_key", "pre", "post", "restore", "stamp")

  def __init__(self, label, coalesce_key, pre, post, restore):
    self.label = label
    self.coalesce_key = coalesce_key
    self.pre = pre
    self.post = post
    self.restore = restore
    self.stamp = time.time()


class UndoStack:
  """Reusable snapshot-based undo/redo. Snapshots are opaque to the stack; the
  host supplies capture semantics at record() time and restore semantics as a
  callable per command (usually one bound method for the whole session)."""

  def __init__(self, limit=100, coalesce_window=1.5, on_change=None):
    self._undo = []
    self._redo = []
    self.limit = int(limit)
    self.coalesce_window = float(coalesce_window)
    self.on_change = on_change          # optional callback() after any stack change

  # ---- recording -------------------------------------------------------------

  def record(self, *, pre, post, restore, label="edit", coalesce_key=None):
    """Record an ALREADY-APPLIED mutation. `pre`/`post` are the host snapshots
    around it; `restore` is the host callable that applies a snapshot. A record
    always clears the redo branch (history is linear)."""
    top = self._undo[-1] if self._undo else None
    if (coalesce_key is not None and top is not None
        and top.coalesce_key == coalesce_key
        and (time.time() - top.stamp) <= self.coalesce_window):
      top.post = post                   # collapse the run: first pre, latest post
      top.stamp = time.time()
    else:
      self._undo.append(SnapshotCommand(label, coalesce_key, pre, post, restore))
      if len(self._undo) > self.limit:
        self._undo.pop(0)
    self._redo.clear()
    self._notify()

  # ---- undo/redo ---------------------------------------------------------------

  def undo(self):
    """Restore the previous snapshot. Returns the undone command's label, or None."""
    if not self._undo:
      return None
    cmd = self._undo.pop()
    cmd.restore(cmd.pre)
    self._redo.append(cmd)
    self._notify()
    return cmd.label

  def redo(self):
    """Re-apply the next snapshot. Returns the redone command's label, or None."""
    if not self._redo:
      return None
    cmd = self._redo.pop()
    cmd.restore(cmd.post)
    self._undo.append(cmd)
    self._notify()
    return cmd.label

  # ---- introspection -----------------------------------------------------------

  def can_undo(self):
    return bool(self._undo)

  def can_redo(self):
    return bool(self._redo)

  def undo_label(self):
    return self._undo[-1].label if self._undo else None

  def redo_label(self):
    return self._redo[-1].label if self._redo else None

  def depth(self):
    return len(self._undo)

  def clear(self):
    """Drop all history (e.g. on opening a different document)."""
    self._undo.clear()
    self._redo.clear()
    self._notify()

  def _notify(self):
    if self.on_change is not None:
      self.on_change()
