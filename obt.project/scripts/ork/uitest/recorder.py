################################################################################
# ork.uitest — recorder (RECORD half, zero new C++)
#
# Taps RAW pre-synthesis ui::Events and snapshots them into a Session. Two taps
# cover every window kind:
#   * app.addGlobalEventHandler  -> main + secondary window events
#     (OrkEzApp::_fireGlobalEvent, fired before Context::handleEvent).
#   * ctx.app_preview_handler    -> popup/overlay events, which bypass the
#     global tap (PopupImpl::_fireEvent). This is Phase-1 of handleEvent and is
#     also pre-synthesis; it must return a NON-handled HandlerResult so routing
#     is left untouched.
#
# A main-window event is seen by BOTH taps (global first, then preview inside
# handleEvent), so the preview tap dedupes against the record the global tap
# just made — see _on_preview_event. Popup events are seen only by the preview
# tap and always record.
#
# Recording is observation-only: it never consumes an event and never mutates
# engine state. Frame correlation is cross-thread: on_update() (update thread)
# stashes updata.counter; the event taps (main thread) stamp the latest value.
################################################################################

from .session import Session, record_from_event, _dumps


class Recorder:
    """Attaches record taps to an OrkEzApp and flushes a Session on stop()."""

    def __init__(self, path=None, screen_dim=None):
        self._path = path
        self._session = Session(screen_dim=screen_dim)
        # cross-thread frame index: written by on_update (update thread), read by
        # the event taps (main thread). A plain int attribute — reference load /
        # store is atomic under both the GIL and free-threaded builds; the tap
        # never needs a torn intermediate, only the latest published counter.
        self._frame_index = 0
        self._app = None
        self._global_token = None
        self._contexts = []
        # canonical key of the record the global tap most recently made; the
        # preview tap suppresses the duplicate main-window event that matches it.
        self._last_global_key = None
        self._handler_result_factory = None

    # -- public API ----------------------------------------------------------

    @property
    def session(self):
        return self._session

    def on_update(self, updata):
        """Call from the app's onUpdate(updata) to publish the frame counter."""
        try:
            self._frame_index = int(updata.counter)
        except Exception:
            pass

    def attach(self, app):
        """Wire the global tap plus a preview tap on every current context."""
        self._app = app
        self._global_token = app.addGlobalEventHandler(self._on_global_event)
        try:
            primary = app.uicontext
        except Exception:
            primary = None
        if primary is not None:
            self.attach_context(primary)
        try:
            secondaries = list(app.secondaryWindows)
        except Exception:
            secondaries = []
        for win in secondaries:
            try:
                ctx = win.ui_context
            except Exception:
                ctx = None
            if ctx is not None:
                self.attach_context(ctx)
        return self

    def attach_context(self, ctx):
        """Install the preview tap on a ui Context (for late-created windows)."""
        ctx.app_preview_handler = self._on_preview_event
        self._contexts.append(ctx)

    def flush(self):
        """Write the session to disk (if a path was given)."""
        if self._path is not None:
            self._session.write(self._path)

    def stop(self):
        """Detach every tap and flush the session."""
        if self._app is not None and self._global_token is not None:
            try:
                self._app.removeGlobalEventHandler(self._global_token)
            except Exception:
                pass
            self._global_token = None
        for ctx in self._contexts:
            try:
                ctx.app_preview_handler = None
            except Exception:
                pass
        self._contexts = []
        self.flush()
        return self._session

    # -- taps ----------------------------------------------------------------

    def _on_global_event(self, ev):
        # main + secondary window events; always recorded (the primary tap).
        rec = self._session.add_event(self._frame_index, ev)
        self._last_global_key = _dumps(rec)
        # global handler return value is ignored by the engine — observation only.

    def _on_preview_event(self, ev):
        # popup/overlay events, plus a second look at main-window events. Build
        # the same canonical record and drop it iff it matches what the global
        # tap just recorded (the main-window duplicate); otherwise it is a
        # popup-only event and gets recorded.
        rec = record_from_event(self._frame_index, ev)
        if _dumps(rec) != self._last_global_key:
            self._session.add_record(rec)
        return self._make_handler_result()

    # -- helpers -------------------------------------------------------------

    def _make_handler_result(self):
        # a default HandlerResult is NON-handled (mHandler==nullptr), so routing
        # is unperturbed. Built lazily so this module imports without lev2.
        if self._handler_result_factory is None:
            try:
                from orkengine import lev2
                self._handler_result_factory = lev2.ui.HandlerResult
            except Exception:
                self._handler_result_factory = False
        if self._handler_result_factory:
            return self._handler_result_factory()
        return None


def record(app, path, screen_dim=None):
    """Attach a Recorder to `app`, writing to `path` on stop().

    The app must forward its update tick: call recorder.on_update(updata) from
    the app's onUpdate(updata), and recorder.stop() when done. Returns the
    attached Recorder.
    """
    rec = Recorder(path=path, screen_dim=screen_dim)
    rec.attach(app)
    return rec
