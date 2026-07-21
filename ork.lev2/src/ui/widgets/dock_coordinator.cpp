////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/ui/dock_coordinator.h>
#include <ork/util/logger.h>
#include <algorithm>
#include <cstdlib>

namespace ork::ui {

static logchannel_ptr_t logchan_dockcoord = logger()->configureChannel("DOCKCOORD", fvec3(0.4, 0.7, 0.9), false);

/////////////////////////////////////////////////////////////////////////
static const char* _kindName(DockDragResolve::Kind k) {
  switch (k) {
    case DockDragResolve::Kind::LOCAL:          return "LOCAL";
    case DockDragResolve::Kind::FOREIGN_ZONE:   return "FOREIGN_ZONE";
    case DockDragResolve::Kind::FOREIGN_NOZONE: return "FOREIGN_NOZONE";
    case DockDragResolve::Kind::TEAROUT:        return "TEAROUT";
    case DockDragResolve::Kind::UNSUPPORTED:    return "UNSUPPORTED";
    default:                                    return "?";
  }
}

/////////////////////////////////////////////////////////////////////////
dockcoordinator_ptr_t DockCoordinator::instance() {
  static dockcoordinator_ptr_t g = std::make_shared<DockCoordinator>();
  return g;
}
/////////////////////////////////////////////////////////////////////////
void DockCoordinator::registerDock(
    const std::string& window_key,
    DockSpace* dock,
    dock_rect_provider_t provider,
    dock_win_number_provider_t win_number_provider) {
  auto it = _entries.find(window_key);
  if (it == _entries.end())
    _order.push_back(window_key);
  Entry e                = (it != _entries.end()) ? it->second : Entry{}; // preserve a pre-set override
  e._dock                = dock;
  e._provider            = provider;
  e._win_number_provider = win_number_provider;
  _entries[window_key]   = e;
}
/////////////////////////////////////////////////////////////////////////
void DockCoordinator::unregisterDock(const std::string& window_key) {
  _entries.erase(window_key);
  _order.erase(std::remove(_order.begin(), _order.end(), window_key), _order.end());
}
/////////////////////////////////////////////////////////////////////////
void DockCoordinator::setWindowRectOverride(const std::string& window_key, int x, int y, int w, int h) {
  if (std::find(_order.begin(), _order.end(), window_key) == _order.end())
    _order.push_back(window_key);
  auto& e        = _entries[window_key]; // creates the entry if absent (dock filled on register)
  e._has_override = true;
  e._override     = Rect(x, y, w, h);
}
/////////////////////////////////////////////////////////////////////////
void DockCoordinator::clearWindowRectOverride(const std::string& window_key) {
  auto it = _entries.find(window_key);
  if (it != _entries.end())
    it->second._has_override = false;
}
/////////////////////////////////////////////////////////////////////////
bool DockCoordinator::_resolveRect(const Entry& e, Rect& out) const {
  if (e._has_override) {
    out = e._override;
    return out._w > 0 && out._h > 0;
  }
  if (e._provider) {
    Rect r = e._provider();
    if (r._w > 0 && r._h > 0) {
      out = r;
      return true;
    }
  }
  return false;
}
/////////////////////////////////////////////////////////////////////////
bool DockCoordinator::_resolveRect(const std::string& window_key, Rect& out) const {
  auto it = _entries.find(window_key);
  if (it == _entries.end())
    return false;
  return _resolveRect(it->second, out);
}
/////////////////////////////////////////////////////////////////////////
std::string DockCoordinator::keyForDock(DockSpace* dock) const {
  for (auto& kv : _entries)
    if (kv.second._dock == dock)
      return kv.first;
  return std::string();
}
/////////////////////////////////////////////////////////////////////////
bool DockCoordinator::_anyRectOverride() const {
  for (auto& kv : _entries)
    if (kv.second._has_override)
      return true;
  return false;
}
/////////////////////////////////////////////////////////////////////////
// BUG-B point-ownership resolution. See the header contract. Priority:
//   1) test override wins (gate seam),
//   2) native leg SKIPPED in synthetic rect-override (offscreen) mode — it
//      queries REAL OS windows and would contradict the synthetic rects,
//   3) native leg (mac): topmost OS window number matched against ours,
//   4) unavailable -> rect-only (nullopt).
/////////////////////////////////////////////////////////////////////////
std::optional<std::string> DockCoordinator::_resolveOwner(int sx, int sy) const {
  if (_point_owner_override)
    return _point_owner_override(sx, sy);
  if (_anyRectOverride())
    return std::nullopt;
  if (_topmost_number_fn) {
    int64_t topnum = _topmost_number_fn(sx, sy);
    if (topnum == 0)
      return std::nullopt; // no resolvable window under the point -> DEGRADE to rect-only
                           //  (safer than forcing tear-out on an undeterminable query)
    for (auto& kv : _entries) {
      const auto& e = kv.second;
      if (e._win_number_provider) {
        int64_t n = e._win_number_provider();
        if (n != 0 && n == topnum)
          return kv.first; // ours, and the actually-topmost window at the point
      }
    }
    return std::string(); // topmost window at the point is some OTHER app's
  }
  return std::nullopt; // ownership unavailable -> caller uses rect-only classification
}
/////////////////////////////////////////////////////////////////////////
// See the coordinate-space contract at the top of dock_coordinator.h: with
// _macosUseHIDPI false (shipped), ui-root == screen points, scale 1.0, so the
// mapping is a pure translation by each window's screen origin.
/////////////////////////////////////////////////////////////////////////
DockDragResolve DockCoordinator::resolveDrag(DockSpace* source, int rx, int ry) {
  DockDragResolve res;

  auto src_key = keyForDock(source);
  if (src_key.empty()) {
    // dock not registered with any window -> pure single-window behavior
    res._kind = DockDragResolve::Kind::LOCAL;
    _traceResolve(res, src_key, "src-unregistered");
    return res;
  }

  ////////////////////////////////////////
  // PINNING (W5): a NON-transferable panel (no registered factory — e.g. a
  //  viewport / node-editor with one-shot Context-bound GPU seams) never leaves
  //  its window. Classify LOCAL regardless of cursor so no foreign hint / tear-out
  //  ever shows and in-window drag behavior is completely unchanged. Unset
  //  predicate => every panel transferable (pre-W5).
  ////////////////////////////////////////
  if (_transferable_pred && source->dragPanel()) {
    auto pid = source->dragPanel()->GetName();
    if (not _transferable_pred(pid)) {
      res._kind = DockDragResolve::Kind::LOCAL;
      _traceResolve(res, src_key, "pinned:" + pid);
      return res;
    }
  }

  Rect src_rect;
  if (not _resolveRect(src_key, src_rect)) {
    if (not _degrade_logged) {
      logchan_dockcoord->log(
          "window positions unavailable for source '%s' — cross-window drag DEGRADED to single-window",
          src_key.c_str());
      _degrade_logged = true;
    }
    res._kind = DockDragResolve::Kind::UNSUPPORTED;
    _traceResolve(res, src_key, "rect-unavailable");
    return res;
  }

  // source ui-root -> screen
  int sx      = src_rect._x + rx;
  int sy      = src_rect._y + ry;
  res._screen_x = sx;
  res._screen_y = sy;

  ////////////////////////////////////////
  // BUG-B point-ownership. When available it AUTHORITATIVELY decides which of
  //  our windows (if any) is under the cursor — occlusion-aware (another app's
  //  window covering ours at a point classifies as NOT ours = tear-out, exactly
  //  like empty desktop) and true-z-order between our OWN overlapping windows
  //  (replacing the registration-order heuristic). Unavailable (non-mac, unwired,
  //  or synthetic rect-override mode) => the rect-only classification below stands.
  ////////////////////////////////////////
  auto owner = _resolveOwner(sx, sy);
  if (owner.has_value()) {
    const std::string& okey = owner.value();
    if (okey.empty()) {
      // topmost OS window under the cursor is NOT ours -> same as empty desktop.
      res._kind = DockDragResolve::Kind::TEAROUT;
      _traceResolve(res, src_key, "owner-not-ours");
      return res;
    }
    if (okey == src_key) {
      res._kind = DockDragResolve::Kind::LOCAL;
      _traceResolve(res, src_key, "owner-source");
      return res;
    }
    auto eit = _entries.find(okey);
    if (eit != _entries.end() && eit->second._dock) {
      Rect r;
      if (_resolveRect(eit->second, r)) {
        int lx   = sx - r._x;
        int ly   = sy - r._y;
        auto hit = eit->second._dock->zoneHitTest(lx, ly);
        res._foreign_dock = eit->second._dock;
        res._foreign_key  = okey;
        if (hit._valid && hit._target) {
          res._kind        = DockDragResolve::Kind::FOREIGN_ZONE;
          res._foreign_hit = hit;
        } else {
          res._kind = DockDragResolve::Kind::FOREIGN_NOZONE;
        }
        _traceResolve(res, src_key, "owner-foreign");
        return res;
      }
    }
    // owner key names no live registered dock -> treat as not ours.
    res._kind = DockDragResolve::Kind::TEAROUT;
    _traceResolve(res, src_key, "owner-unresolved");
    return res;
  }

  ////////////////////////////////////////
  // ownership UNAVAILABLE: rect-only classification (pre-BUG-B behavior).
  ////////////////////////////////////////

  // inside the source window's OWN bounds -> local behavior
  if (rx >= 0 && rx < src_rect._w && ry >= 0 && ry < src_rect._h) {
    res._kind = DockDragResolve::Kind::LOCAL;
    _traceResolve(res, src_key, "in-source-bounds");
    return res;
  }

  ////////////////////////////////////////
  // topmost registered window under the screen cursor. v1 iteration order:
  //  registration order reversed (secondaries newest-first, then main, which is
  //  registered first). Overlapping windows are resolved by this order only — a
  //  documented v1 limitation (no true z-order query).
  ////////////////////////////////////////
  for (auto it = _order.rbegin(); it != _order.rend(); ++it) {
    const auto& key = *it;
    if (key == src_key)
      continue;
    auto eit = _entries.find(key);
    if (eit == _entries.end())
      continue;
    auto dock = eit->second._dock;
    if (not dock)
      continue;
    Rect r;
    if (not _resolveRect(eit->second, r))
      continue;
    if (sx >= r._x && sx < r._x + r._w && sy >= r._y && sy < r._y + r._h) {
      // screen -> target ui-root
      int lx    = sx - r._x;
      int ly    = sy - r._y;
      auto hit  = dock->zoneHitTest(lx, ly);
      res._foreign_dock = dock;
      res._foreign_key  = key;
      if (hit._valid && hit._target) {
        res._kind        = DockDragResolve::Kind::FOREIGN_ZONE;
        res._foreign_hit = hit;
      } else {
        res._kind = DockDragResolve::Kind::FOREIGN_NOZONE;
      }
      _traceResolve(res, src_key, "foreign-window");
      return res;
    }
  }

  // outside every registered window -> tear-out onto empty desktop
  res._kind = DockDragResolve::Kind::TEAROUT;
  _traceResolve(res, src_key, "outside-all-windows");
  return res;
}
/////////////////////////////////////////////////////////////////////////
void DockCoordinator::_traceResolve(
    const DockDragResolve& res, const std::string& src_key, const std::string& reason) {
  if (_trace_enabled < 0) {
    const char* e  = std::getenv("ORKID_DOCK_TRACE");
    _trace_enabled = (e && e[0] && e[0] != '0') ? 1 : 0;
  }
  if (not _trace_enabled)
    return;

  const char* kindstr = _kindName(res._kind);
  // dedup key: emit only when the classification (kind + foreign target + reason)
  // actually changes across the many resolveDrag calls of one drag.
  std::string detail = FormatString("%s|%s|%s", kindstr, res._foreign_key.c_str(), reason.c_str());
  if (_traced_once && detail == _last_trace_detail)
    return;
  _traced_once      = true;
  _last_trace_detail = detail;

  std::string line = FormatString("resolveDrag %s src='%s'", kindstr, src_key.c_str());

  Rect sr;
  if (not src_key.empty() && _resolveRect(src_key, sr))
    line += FormatString(" srcRect=(%d,%d,%d,%d)", sr._x, sr._y, sr._w, sr._h);

  if (res._kind == DockDragResolve::Kind::FOREIGN_ZONE ||
      res._kind == DockDragResolve::Kind::FOREIGN_NOZONE) {
    line += FormatString(" dst='%s'", res._foreign_key.c_str());
    Rect dr;
    if (_resolveRect(res._foreign_key, dr))
      line += FormatString(" dstRect=(%d,%d,%d,%d)", dr._x, dr._y, dr._w, dr._h);
  }
  if (res._screen_x != 0 || res._screen_y != 0)
    line += FormatString(" cursor=(%d,%d)", res._screen_x, res._screen_y);
  line += FormatString(" [%s]", reason.c_str());

  // printf (not the DOCKCOORD logchannel, which ships disabled): ORKID_DOCK_TRACE
  // must reliably reach stdout for the owner's live-test diagnostic.
  printf("[DOCKTRACE] %s\n", line.c_str());
  fflush(stdout);
}
/////////////////////////////////////////////////////////////////////////
void DockCoordinator::invokeTransfer(
    const std::string& panel_id,
    const std::string& src_key,
    const std::string& dst_key,
    const std::string& target_panel_id,
    const std::string& zone) {
  if (_transfer_cb)
    _transfer_cb(panel_id, src_key, dst_key, target_panel_id, zone);
}
/////////////////////////////////////////////////////////////////////////
void DockCoordinator::invokeTearOut(const std::string& panel_id, const std::string& src_key, int screen_x, int screen_y) {
  if (_tearout_cb)
    _tearout_cb(panel_id, src_key, screen_x, screen_y);
}
/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
