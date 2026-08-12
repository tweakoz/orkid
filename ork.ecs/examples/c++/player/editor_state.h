#pragma once
////////////////////////////////////////////////////////////////
// editor_state — the player's SAVED EDITOR VALUES: what the HUD's editor pages hold,
// written beside the scene that authored them and read back the next time that scene runs.
//
// THREE TIERS, and the file is only the third:
//
//   1 GLOBAL DEFAULT   the engine's own defaults — a default-constructed PostFxNodeACES /
//                      PostFxNodeHSVG / SkyAtmosphereData. The player materializes this
//                      tier for any scene that declares nothing (main.cpp's post-chain
//                      self-defense), which is what makes every scene editable at all.
//   2 SCENE AUTHORED   what scn_XXX.py declared, deserialized from the .ecs. Tier 2 is
//                      simply "what the objects hold at bind", before anything here runs.
//   3 SAVED            this file, applied over tier 2 through the SAME set() closures the
//                      HUD rows drive. No file = tiers 1+2 exactly, byte for byte.
//
// A SAVED VALUE NEVER SHADOWS A RE-AUTHORED SCENE. Every entry carries `was` — the tier-2
// value that was live when it was saved. At load, `was` is compared against tier 2 NOW:
// equal means the scene has not moved and the saved value applies; different means the
// author changed the scene since, so TIER 2 WINS and the divergence is reported by name.
// That is the whole defense against a months-old file quietly reverting a fresh edit.
//
// PER PRESENTATION MODE. Desktop and VR are graded differently (the owner's premise), so
// the file carries one section per mode and a run touches only its own. A VR run with no
// VR section starts from tiers 1+2 — it does NOT inherit the desktop grade, because
// seeding VR from desktop would silently reproduce the look the split exists to escape.
//
// TOLERANT BY CONSTRUCTION: a prop in the file that no page has, a page that no longer
// exists, a renamed row, a missing entry — all are reported by name and skipped. Nothing
// here can abort a run over a stale state file. A file that will not PARSE is the one
// hard stop, and it stops the SAVE (never truncate what you could not read), not the run.
//
// Deliberately free of gfx / ezapp includes, like perfhud_pages.h: this is a pure function
// of the page model plus a path, so ork.ecs/tests/perfhud_pages.cpp exercises it off-device.
////////////////////////////////////////////////////////////////

#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include "perfhud_pages.h"

namespace ork::ecs::player {

////////////////////////////////////////////////////////////////

static constexpr int kEditorStateSchema = 1;

// One saved (or baselined) value, in the kind's own currency: a float for FLOAT, a LABEL
// for ENUM (with the engine's crc for it — the label is what a human reads and what the
// file stores, the crc is how a read resolves it), an rgb triple for COLOR.
struct EditorValue {
  HudEditProp::Kind _kind = HudEditProp::FLOAT;
  float             _f    = 0.0f;
  HudRGB            _rgb  = {0.0f, 0.0f, 0.0f};
  std::string       _enum_label;
  uint64_t          _enum_crc = 0;

  bool sameAs(const EditorValue& o) const {
    if (_kind != o._kind)
      return false;
    switch (_kind) {
      case HudEditProp::COLOR:
        // an exact compare would call a value re-derived through HSV "changed"; the
        // tolerance is far below a visible step and far below any real re-authoring.
        for (int i = 0; i < 3; i++)
          if (std::fabs(_rgb[i] - o._rgb[i]) > 1.0e-5f)
            return false;
        return true;
      case HudEditProp::ENUM: return _enum_crc == o._enum_crc;
      default:                return std::fabs(_f - o._f) <= 1.0e-6f;
    }
  }
};

// PROP ID -> value. Deliberately FLAT, with no page level: pages are presentation, and
// moving a row from one page to another must never orphan or rekey what the owner saved.
// The id is the row's label, unique across the ring by construction (capture() reports a
// collision rather than letting one row silently shadow another).
using EditorValueMap = std::map<std::string, EditorValue>;

struct EditorStateReport {
  bool                     _ok       = true;
  int                      _applied  = 0;
  int                      _skipped  = 0; // scene re-authored since the save — tier 2 kept
  int                      _unknown  = 0; // page/row in the file that no longer exists
  int                      _written  = 0;
  std::string              _summary;
  std::vector<std::string> _lines; // one human line per anomaly, for the host to print
};

////////////////////////////////////////////////////////////////
// The mode a run saves and loads under. It is the RENDER MODEL, not the flag: --vr with a
// live runtime and --vr on a NoVr device both present stereo through the VR preset, and
// both want the VR grade.
////////////////////////////////////////////////////////////////

inline const char* editorStateModeKey(bool vr_mode) {
  return vr_mode ? "vr" : "desktop";
}

////////////////////////////////////////////////////////////////

struct EditorStateIO {

  // Sidecar for a scene source: <dir of scn_XXX.py>/scn_XXX.json. Derived, never stored —
  // the scene source itself is a path TOKEN expanded against the live workspace, so this
  // lands in the local checkout on whatever machine is running.
  static std::string sidecarPath(const std::string& scene_py_abs) {
    auto slash = scene_py_abs.find_last_of('/');
    auto dot   = scene_py_abs.find_last_of('.');
    if (dot == std::string::npos or (slash != std::string::npos and dot < slash))
      return scene_py_abs + ".json";
    return scene_py_abs.substr(0, dot) + ".json";
  }

  //////////////////////////////////////////////////////////////
  // TIER-2 BASELINE: what every editable row holds right now. Called once at bind, BEFORE
  // any saved value is applied, so it is the scene-authored truth this file measures
  // against. ACTION rows hold no value and are skipped; `exclude` drops rows whose getter
  // cannot tell the truth (see the time-of-day note in main.cpp).
  //////////////////////////////////////////////////////////////

  static EditorValueMap capture(
      const PerfHudPages&             pages,
      const std::vector<std::string>& exclude   = {},
      std::vector<std::string>*       anomalies = nullptr) {
    EditorValueMap out;
    for (int p = PerfHudPages::NUM_PAGES; p < pages.numPages(); p++) {
      const auto* ep = pages.editorPage(p);
      if (not ep)
        continue;
      for (const auto& P : ep->_props) {
        // NOTHING EDITABLE IS EVER EXCLUDED (owner law). An ACTION is the one kind that
        // is skipped, and it is not an exclusion: a command row holds no value, so there
        // is nothing about it to write down. Every other skip below REPORTS, because a row
        // that quietly stops persisting is indistinguishable from one that never worked.
        if (P._kind == HudEditProp::ACTION)
          continue;
        bool skip = false;
        for (const auto& x : exclude)
          if (x == P._label)
            skip = true;
        if (skip) {
          if (anomalies)
            anomalies->push_back(
                "editor row <" + P._label + "> is on an exclusion list — it will NOT be saved "
                "or restored; nothing editable should be excluded, so this is a defect to raise");
          continue;
        }
        EditorValue v;
        if (not _read(P, v)) {
          // A row the model cannot READ cannot be saved: a FLOAT/ENUM with no getter, a
          // COLOR with no color getter, an ENUM with an empty choice list. It is a wiring
          // bug in whoever registered the row, and it is loud here rather than a value the
          // owner sets, saves, and never sees again.
          if (anomalies)
            anomalies->push_back(
                "editor row <" + P._label + "> has no readable getter — it cannot be saved "
                "or restored; the row's registration is incomplete");
          continue;
        }
        // TWO ROWS, ONE ID would take turns overwriting each other in the file. Report it
        // and persist the first; the collision is a naming bug, not a user's problem.
        if (out.count(P._label)) {
          if (anomalies)
            anomalies->push_back(
                "two editor rows share the id <" + P._label +
                "> — only the first persists; give one a distinct label");
          continue;
        }
        out[P._label] = v;
      }
    }
    return out;
  }

  // every editable row on the ring, by id — the lookup a page-agnostic file needs.
  static HudEditProp* findProp(PerfHudPages& pages, const std::string& id) {
    for (int p = PerfHudPages::NUM_PAGES; p < pages.numPages(); p++) {
      auto* ep = pages.editorPage(p);
      if (not ep)
        continue;
      for (auto& P : ep->_props)
        if (P._label == id)
          return &P;
    }
    return nullptr;
  }

  //////////////////////////////////////////////////////////////
  // READ, applying nothing. The whole of load() except the final write, split out because
  // one saved value is consumed BEFORE the thing that holds it exists: the character's
  // SPAWN is written into the SceneData ahead of createSimulation (a spawned-there
  // character never fights physics the way a mid-run teleport does), and at that moment
  // there is no live row closure to push it through. Everything else — the mode section,
  // the re-authoring rule, the per-row reporting — is identical, so a value read this way
  // and a value applied by load() can never disagree about what the file says.
  //////////////////////////////////////////////////////////////

  static EditorValueMap readSaved(
      const std::string&    path,
      bool                  vr_mode,
      PerfHudPages&         pages,
      const EditorValueMap& baselines,
      EditorStateReport&    rep) {
    EditorValueMap out;
    std::string    text;
    if (not _readFile(path, text)) {
      rep._summary = "no saved editor values";
      return out; // NOT an error: no file is the default state of every scene
    }
    rapidjson::Document doc;
    doc.Parse(text.c_str());
    if (doc.HasParseError() or not doc.IsObject()) {
      rep._ok      = false;
      rep._summary = "unreadable — left untouched";
      rep._lines.push_back("editor state " + path + " does not parse; not applied, and a save will refuse");
      return out;
    }
    auto mi = doc.FindMember("modes");
    if (mi == doc.MemberEnd() or not mi->value.IsObject()) {
      rep._summary = "no mode sections";
      return out;
    }
    const char* mode = editorStateModeKey(vr_mode);
    auto        si   = mi->value.FindMember(mode);
    if (si == mi->value.MemberEnd() or not si->value.IsObject()) {
      // The other mode may well have values; say so rather than inheriting them.
      const char* other = editorStateModeKey(not vr_mode);
      bool        has_other = mi->value.HasMember(other);
      rep._summary = std::string("no ") + mode + " values";
      if (has_other)
        rep._lines.push_back(
            std::string("editor state has ") + other + " values but none for " + mode +
            " — not applied (a mode is graded on its own, never inherited)");
      return out;
    }
    // PAGE-AGNOSTIC: one flat map of prop ids. A row that moved from one page to another
    // between runs is the same row here, which is the whole point of dropping the page
    // level — presentation may be reorganized freely without touching saved values.
    auto pi = si->value.FindMember("props");
    if (pi == si->value.MemberEnd() or not pi->value.IsObject()) {
      rep._summary = std::string("no ") + mode + " values";
      return out;
    }
    for (auto ri = pi->value.MemberBegin(); ri != pi->value.MemberEnd(); ++ri) {
      std::string  id   = ri->name.GetString();
      HudEditProp* prop = findProp(pages, id);
      if (not prop or not ri->value.IsObject()) {
        rep._unknown++;
        rep._lines.push_back("editor state names <" + id + ">, which this scene has no row for — skipped");
        continue;
      }
      EditorValue saved, was;
      if (not _parseEntry(ri->value, *prop, saved, was)) {
        rep._unknown++;
        rep._lines.push_back("editor state entry <" + id + "> is malformed — skipped");
        continue;
      }
      // TIER 2 NOW vs the tier 2 that was live at save time.
      auto br = baselines.find(id);
      if (br != baselines.end() and not br->second.sameAs(was)) {
        rep._skipped++;
        rep._lines.push_back(
            "scene re-authored <" + id + "> since the save (" + _describe(was) + " -> " +
            _describe(br->second) + ") — keeping the scene's value");
        continue;
      }
      out[id] = saved;
    }
    return out;
  }

  //////////////////////////////////////////////////////////////
  // LOAD + APPLY. Tier 3 over tier 2, per the re-authoring rule above: the read above,
  // then one write per surviving row.
  //////////////////////////////////////////////////////////////

  static EditorStateReport load(
      const std::string&    path,
      bool                  vr_mode,
      PerfHudPages&         pages,
      const EditorValueMap& baselines) {
    EditorStateReport rep;
    auto              saved = readSaved(path, vr_mode, pages, baselines, rep);
    if (not rep._summary.empty())
      return rep; // no file / unreadable / no section for this mode — readSaved said why
    for (const auto& item : saved) {
      auto* prop = findProp(pages, item.first);
      if (prop and _write(*prop, item.second)) {
        rep._applied++;
        // NAME EVERY ROW IT TOUCHED. A count alone cannot answer the only question that
        // matters when a reloaded scene looks unchanged — which value was restored, and to
        // what — so the load says it outright and the next run is its own evidence.
        rep._lines.push_back("restored <" + item.first + "> = " + _describe(item.second));
      }
      else {
        rep._unknown++;
        rep._lines.push_back("editor state entry <" + item.first + "> could not be applied (kind changed) — skipped");
      }
    }
    char b[128];
    snprintf(b, sizeof(b), "%d applied, %d kept by the scene, %d unknown", rep._applied, rep._skipped, rep._unknown);
    rep._summary = b;
    return rep;
  }

  //////////////////////////////////////////////////////////////
  // SAVE. Only rows that DIFFER from tier 2 are written (the file is a diff against the
  // scene, not a second copy of it), only this mode's section is replaced, and the write
  // is atomic — a crash mid-save must never leave half a file where a scene's look was.
  //////////////////////////////////////////////////////////////

  static EditorStateReport save(
      const std::string&    path,
      bool                  vr_mode,
      const std::string&    scene_name,
      const PerfHudPages&   pages,
      const EditorValueMap& baselines) {
    EditorStateReport rep;
    rapidjson::Document doc;
    std::string         text;
    bool                had_file = _readFile(path, text);
    if (had_file) {
      doc.Parse(text.c_str());
      if (doc.HasParseError() or not doc.IsObject()) {
        rep._ok      = false;
        rep._summary = "existing file does not parse — refusing to overwrite";
        rep._lines.push_back("editor state " + path + " is not valid json; the other mode's values would be lost, so nothing was written");
        return rep;
      }
    } else {
      doc.SetObject();
    }
    auto& al = doc.GetAllocator();
    _setMember(doc, al, "schema", rapidjson::Value(kEditorStateSchema));
    _setMember(doc, al, "scene", rapidjson::Value(scene_name.c_str(), al));
    if (not doc.HasMember("modes") or not doc["modes"].IsObject())
      _setMember(doc, al, "modes", rapidjson::Value(rapidjson::kObjectType));

    rapidjson::Value props(rapidjson::kObjectType);
    for (int p = PerfHudPages::NUM_PAGES; p < pages.numPages(); p++) {
      const auto* ep = pages.editorPage(p);
      if (not ep)
        continue;
      for (const auto& P : ep->_props) {
        auto br = baselines.find(P._label);
        if (br == baselines.end()) // not baselined = not persistable (ACTION, excluded, dup)
          continue;
        if (props.HasMember(P._label.c_str()))
          continue;
        EditorValue now;
        if (not _read(P, now))
          continue;
        if (now.sameAs(br->second))
          continue; // unedited — the scene already says this
        rapidjson::Value entry(rapidjson::kObjectType);
        _emitValue(entry, al, "v", "rgb", now);
        _emitValue(entry, al, "was", "was_rgb", br->second);
        rapidjson::Value key(P._label.c_str(), al);
        props.AddMember(key, entry, al);
        rep._written++;
      }
    }
    rapidjson::Value section(rapidjson::kObjectType);
    section.AddMember(rapidjson::Value("props", al), props, al);
    auto& modes = doc["modes"];
    const char* mode = editorStateModeKey(vr_mode);
    if (modes.HasMember(mode))
      modes.RemoveMember(mode);
    rapidjson::Value mkey(mode, al);
    modes.AddMember(mkey, section, al);

    rapidjson::StringBuffer                          sb;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> w(sb);
    w.SetIndent(' ', 2);
    doc.Accept(w);
    if (not _writeFileAtomic(path, std::string(sb.GetString()))) {
      rep._ok      = false;
      rep._summary = "write failed";
      rep._lines.push_back("editor state could not be written to " + path + " (permissions? read-only checkout?)");
      return rep;
    }
    char b[128];
    snprintf(b, sizeof(b), "%d value(s) -> %s", rep._written, mode);
    rep._summary = b;
    return rep;
  }

  //////////////////////////////////////////////////////////////

private:
  static bool _read(const HudEditProp& P, EditorValue& out) {
    out._kind = P._kind;
    switch (P._kind) {
      case HudEditProp::COLOR:
        if (not P._getc)
          return false;
        out._rgb = P._getc();
        return true;
      case HudEditProp::ENUM: {
        if (not P._get or P._enum.empty())
          return false;
        int n   = int(P._enum.size());
        int idx = std::min(n - 1, std::max(0, int(std::lround(P._get()))));
        out._f          = float(idx);
        out._enum_label = P._enum[idx]._label;
        out._enum_crc   = P._enum[idx]._crc;
        return true;
      }
      case HudEditProp::ACTION: return false;
      default:
        if (not P._get)
          return false;
        out._f = P._get();
        return true;
    }
  }

  static bool _write(HudEditProp& P, const EditorValue& v) {
    if (P._kind != v._kind)
      return false;
    switch (P._kind) {
      case HudEditProp::COLOR:
        if (not P._setc)
          return false;
        P._setc(v._rgb);
        return true;
      case HudEditProp::ENUM: {
        if (not P._set)
          return false;
        // resolve the saved LABEL through the engine's crc, not a string compare
        for (size_t i = 0; i < P._enum.size(); i++)
          if (P._enum[i]._crc == v._enum_crc) {
            P._set(float(i));
            return true;
          }
        return false;
      }
      case HudEditProp::ACTION: return false;
      default:
        if (not P._set)
          return false;
        P._set(std::min(P._max, std::max(P._min, v._f)));
        return true;
    }
  }

  static void _emitValue(
      rapidjson::Value&                entry,
      rapidjson::Document::AllocatorType& al,
      const char*                      scalar_key,
      const char*                      rgb_key,
      const EditorValue&               v) {
    if (v._kind == HudEditProp::COLOR) {
      rapidjson::Value arr(rapidjson::kArrayType);
      for (int i = 0; i < 3; i++)
        arr.PushBack(v._rgb[i], al);
      entry.AddMember(rapidjson::Value(rgb_key, al), arr, al);
    } else if (v._kind == HudEditProp::ENUM) {
      entry.AddMember(rapidjson::Value(scalar_key, al), rapidjson::Value(v._enum_label.c_str(), al), al);
    } else {
      entry.AddMember(rapidjson::Value(scalar_key, al), rapidjson::Value(v._f), al);
    }
  }

  static bool _parseOne(
      const rapidjson::Value& entry,
      const HudEditProp&      P,
      const char*             scalar_key,
      const char*             rgb_key,
      EditorValue&            out) {
    out._kind = P._kind;
    if (P._kind == HudEditProp::COLOR) {
      auto it = entry.FindMember(rgb_key);
      if (it == entry.MemberEnd() or not it->value.IsArray() or it->value.Size() != 3)
        return false;
      for (int i = 0; i < 3; i++) {
        if (not it->value[i].IsNumber())
          return false;
        out._rgb[i] = float(it->value[i].GetDouble());
      }
      return true;
    }
    auto it = entry.FindMember(scalar_key);
    if (it == entry.MemberEnd())
      return false;
    if (P._kind == HudEditProp::ENUM) {
      if (not it->value.IsString())
        return false;
      out._enum_label = it->value.GetString();
      out._enum_crc   = ork::CrcString(out._enum_label.c_str()).hashed();
      return true;
    }
    if (not it->value.IsNumber())
      return false;
    out._f = float(it->value.GetDouble());
    return true;
  }

  static bool _parseEntry(
      const rapidjson::Value& entry,
      const HudEditProp&      P,
      EditorValue&            saved,
      EditorValue&            was) {
    if (not entry.IsObject())
      return false;
    if (not _parseOne(entry, P, "v", "rgb", saved))
      return false;
    // `was` is what makes the re-authoring check possible; a file without it is still
    // readable, and simply loses that protection for the row.
    if (not _parseOne(entry, P, "was", "was_rgb", was))
      was = saved;
    return true;
  }

  static std::string _describe(const EditorValue& v) {
    char b[96];
    if (v._kind == HudEditProp::COLOR)
      snprintf(b, sizeof(b), "rgb %.4g %.4g %.4g", double(v._rgb[0]), double(v._rgb[1]), double(v._rgb[2]));
    else if (v._kind == HudEditProp::ENUM)
      snprintf(b, sizeof(b), "%s", v._enum_label.c_str());
    else
      snprintf(b, sizeof(b), "%.6g", double(v._f));
    return b;
  }

  static void _setMember(
      rapidjson::Document&                doc,
      rapidjson::Document::AllocatorType& al,
      const char*                         key,
      rapidjson::Value&&                  val) {
    if (doc.HasMember(key))
      doc.RemoveMember(key);
    doc.AddMember(rapidjson::Value(key, al), val, al);
  }

  static bool _readFile(const std::string& path, std::string& out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (not f)
      return false;
    char   buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
      out.append(buf, n);
    fclose(f);
    return true;
  }

  // tmp + rename in the SAME directory: rename is atomic there, so a reader either sees
  // the whole previous file or the whole new one, never a truncated middle.
  static bool _writeFileAtomic(const std::string& path, const std::string& text) {
    std::string tmp = path + ".tmp";
    FILE*       f   = fopen(tmp.c_str(), "wb");
    if (not f)
      return false;
    bool ok = fwrite(text.data(), 1, text.size(), f) == text.size();
    fflush(f);
    fclose(f);
    if (not ok) {
      remove(tmp.c_str());
      return false;
    }
    if (rename(tmp.c_str(), path.c_str()) != 0) {
      remove(tmp.c_str());
      return false;
    }
    return true;
  }
};

} // namespace ork::ecs::player
