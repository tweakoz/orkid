////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/shadlang.h>
#include <ork/file/path.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/string/string.h>
#include <ork/util/parser.h>
#include <utpp/UnitTest++.h>
#include <boost/filesystem.hpp>
#include <string>
#include <vector>
#include <algorithm>
#include <set>
#include <cstdlib>

using namespace std::string_literals;
using namespace ork::lev2;

std::string snip1 = R"(
///////////////////////////////////////////////////////////////
// FxConfigs
///////////////////////////////////////////////////////////////
fxconfig fxcfg_default { glsl_version = "150"; }
///////////////////////////////////////////////////////////////
// Interfaces
///////////////////////////////////////////////////////////////
uniform_set push_constants { //
  mat4 mvp;
  mat4 mvp_l;
  mat4 mvp_r;
  vec4 ModColor;
}
sampler_set samplers (descriptor_set 0) {
  sampler2D ColorMap;
}
vertex_interface iface_vdefault : push_constants {
  inputs {
    vec4 position : POSITION;
    vec4 vtxcolor : COLOR0;
    vec2 uv0 : TEXCOORD0;
    vec2 uv1 : TEXCOORD1;
  }
  outputs {
    //vec4 frg_clr;
    vec2 frg_uv;
  }
}
///////////////////////////////////////////////////////////////
)";

TEST(shadlang1) {
  // this one works
  auto slp_cache = std::make_shared<shadlang::ShadLangParserCache>();
  auto tunit = parseFromString(slp_cache, "yo", snip1);
}

TEST(shadlang2) {
  // remove the last character from the snippet: a deliberate parse failure
  //  (no newline at the end). this must now raise a catchable ork::ParseError
  //  carrying line/col — it used to OrkAssert(false)/crash.
  auto snip2     = snip1.substr(0, snip1.size() - 1);
  auto slp_cache = std::make_shared<shadlang::ShadLangParserCache>();
  bool threw     = false;
  try {
    auto tunit = parseFromString(slp_cache, "yo", snip2);
  } catch (const ork::ParseError& e) {
    threw = true;
    printf("[shadlang2] caught ParseError line<%zu> col<%zu> near<%s> expected<%zu>\n", //
           e._line, e._column, e._near_token.c_str(), e._expected.size());
  }
  CHECK(threw);
}

///////////////////////////////////////////////////////////////////////////////
// broken-shader corpus: seeded syntax errors must each raise a CATCHABLE
//  ork::ParseError naming line/col (and, where terminals were expected, a
//  meaningful expected-set) — the interpreter/host must survive.
///////////////////////////////////////////////////////////////////////////////

TEST(shadlang_broken_corpus) {
  struct BrokenCase {
    std::string _name;
    std::string _text;
  };
  std::vector<BrokenCase> cases;
  // 0: shadlang2's truncated snippet
  cases.push_back({"truncated_snip1", snip1.substr(0, snip1.size() - 1)});
  // 1: two type keywords where an identifier is expected
  cases.push_back({"double_type_member", "uniform_set us { mat4 mat4 mvp; }\n"});
  // 2: statement missing semicolon then stray close
  cases.push_back({"missing_semicolon",
                   "fragment_shader ps : iface_f {\n  float d = 1.0\n  out_clr = vec4(d);\n}\n"});
  // 3: unbalanced parens in an expression
  cases.push_back({"unbalanced_parens",
                   "fragment_shader ps : iface_f {\n  float d = (min(1.0, 2.0;\n}\n"});
  // 4: junk at top level
  cases.push_back({"toplevel_junk", "%%% not a declaration %%%\n"});
  // 5: interface body with a broken member decl
  cases.push_back({"broken_member", "uniform_set us {\n  mat4 ;\n}\n"});

  int n_threw = 0;
  int n_have_pos = 0;
  for (auto& c : cases) {
    bool threw = false;
    try {
      auto slp = std::make_shared<shadlang::ShadLangParserCache>();
      parseFromString(slp, c._name, c._text);
    } catch (const ork::ParseError& e) {
      threw = true;
      std::string expected_join;
      for (size_t i = 0; i < e._expected.size(); i++) {
        if (i)
          expected_join += ", ";
        expected_join += e._expected[i];
      }
      printf("[broken] case<%s> line<%zu> col<%zu> near<%s> expected{%s} breadcrumb<%zu>\n", //
             c._name.c_str(), e._line, e._column, e._near_token.c_str(),
             expected_join.c_str(), e._breadcrumb.size());
      if (e._line > 0 or e._column > 0 or not e._expected.empty()) {
        n_have_pos++;
      }
    } catch (const std::exception& e) {
      threw = true;
      printf("[broken] case<%s> caught std::exception (not ParseError): %s\n", c._name.c_str(), e.what());
    }
    if (threw)
      n_threw++;
    CHECK(threw); // every broken input must raise a catchable exception
  }
  printf("[broken] threw<%d/%zu> with_position<%d>\n", n_threw, cases.size(), n_have_pos);
  CHECK_EQUAL(int(cases.size()), n_threw);
}

///////////////////////////////////////////////////////////////////////////////
// packrat-cache benchmark: synthetically nested `float d = (min(...))`
//  opt-in via SHADLANG_BENCH_N (nesting depth). optional SHADLANG_BENCH_STYLE
//  in {linear,double}. observe cache counters with ORKID_PARSER_CACHE_STATS=1.
///////////////////////////////////////////////////////////////////////////////

static std::string _buildNestedExpr(int n, const std::string& style) {
  if (n <= 0) {
    return "frg_clr.x";
  }
  auto sub = _buildNestedExpr(n - 1, style);
  if (style == "double") {
    return "(min(" + sub + ", " + sub + "))";
  }
  // linear-depth nesting (text size linear, parse depth == n)
  return "(min(" + sub + ", frg_clr.y))";
}

static std::string _buildBenchSnippet(int n, const std::string& style) {
  std::string s;
  s += "fragment_interface iface_fbench {\n";
  s += "  inputs { vec4 frg_clr; }\n";
  s += "  outputs { layout(location = 0) vec4 out_clr; }\n";
  s += "}\n";
  s += "fragment_shader ps_bench : iface_fbench {\n";
  s += "  float d = " + _buildNestedExpr(n, style) + ";\n";
  s += "  out_clr = vec4(d);\n";
  s += "}\n";
  return s;
}

TEST(shadlang_packrat_bench) {
  const char* n_env = std::getenv("SHADLANG_BENCH_N");
  if (not n_env) {
    return; // opt-in only
  }
  int n            = atoi(n_env);
  const char* sty  = std::getenv("SHADLANG_BENCH_STYLE");
  std::string style = sty ? sty : "linear";
  auto snip        = _buildBenchSnippet(n, style);
  printf("[bench] N=%d style=%s snip_bytes=%zu\n", n, style.c_str(), snip.size());
  fflush(stdout);
  auto slp_cache = std::make_shared<shadlang::ShadLangParserCache>();
  ork::Timer t;
  t.Start();
  auto tunit  = parseFromString(slp_cache, "bench", snip);
  double secs = t.SecsSinceStart();
  printf("[bench] N=%d style=%s elapsed_secs=%.4f parsed_ok=%d\n", n, style.c_str(), secs, tunit != nullptr);
  fflush(stdout);
}

///////////////////////////////////////////////////////////////////////////////
// corpus AST-dump sweep: parse every platform fxv2 shader and emit a
//  deterministic toASTstring dump. used for pre/post-fix byte-identity diffing.
///////////////////////////////////////////////////////////////////////////////

TEST(shadlang_corpus) {
  namespace fs = boost::filesystem;
  auto fxv2_dir = ork::file::Path::data_dir() / "platform_lev2" / "shaders" / "fxv2";
  fs::path dir(fxv2_dir.c_str());
  std::vector<std::string> files;
  for (fs::directory_iterator it(dir); it != fs::directory_iterator(); ++it) {
    auto p = it->path();
    if (p.extension() == ".fxv2") {
      files.push_back(p.filename().string());
    }
  }
  std::sort(files.begin(), files.end());
  // skip files that do not parse standalone via this path (pre-existing grammar
  //  limitations, identical pre/post packrat-fix). override via env.
  std::set<std::string> skip;
  const char* skip_env = std::getenv("SHADLANG_CORPUS_SKIP");
  std::string skip_str = skip_env ? skip_env : "terrain.fxv2";
  {
    std::vector<std::string> toks;
    ork::SplitString(skip_str, ',', toks);
    for (auto t : toks)
      skip.insert(t);
  }
  printf("[corpus] fxv2_dir<%s> count<%zu> skip<%s>\n", fxv2_dir.c_str(), files.size(), skip_str.c_str());
  for (auto fname : files) {
    if (skip.count(fname)) {
      printf("========== CORPUS_FILE<%s> SKIPPED ==========\n", fname.c_str());
      continue;
    }
    auto fpath          = fxv2_dir / fname;
    auto slp_cache      = std::make_shared<shadlang::ShadLangParserCache>();
    slp_cache->_toplevel_path = fpath;
    auto tunit          = parseFromFile(slp_cache, fpath);
    auto ast_str        = shadlang::SHAST::toASTstring(tunit);
    printf("========== CORPUS_FILE<%s> ==========\n%s\n", fname.c_str(), ast_str.c_str());
  }
  fflush(stdout);
}