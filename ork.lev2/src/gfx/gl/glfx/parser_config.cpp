////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "../gl.h"
#include "glslfxi.h"
#include "glslfxi_parser.h"
#include <ork/file/file.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/pch.h>
#include <regex>
#include <stdlib.h>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////

void ConfigNode::parse(const ScannerView& view) {
  NamedBlockNode::parse(view);
  ///////////////////////////////////
  int ist = view._start + 1;
  int ien = view._end - 1;
  std::vector<std::string> imports;
  for (size_t i = ist; i <= ien;) {
    const Token* vt_tok = view.token(i);
    if (vt_tok->text == "import") {
      const Token* impnam = view.token(i + 1);
      std::string p       = impnam->text.substr(1, impnam->text.length() - 2);
      imports.push_back(p);
      i += 3;
    } else
      i++;
  }
  ///////////////////////////////////
  // handle imports
  ///////////////////////////////////
  for (const auto& imp : imports) {
    Scanner import_scanner(block_regex);
    file::Path::NameType a, b;
    _container->_path.Split(a, b, ':');
    ork::FixedString<256> fxs;
    fxs.format("%s://%s", a.c_str(), imp.c_str());
    file::Path imppath = fxs.c_str();
    ///////////////////////////////////
    File fx_file(imppath.c_str(), EFM_READ);
    OrkAssert(fx_file.IsOpen());
    EFileErrCode eFileErr = fx_file.GetLength(import_scanner.ifilelen);
    OrkAssert(import_scanner.ifilelen < import_scanner.kmaxfxblen);
    eFileErr                             = fx_file.Read(import_scanner.fxbuffer, import_scanner.ifilelen);
    import_scanner.fxbuffer[import_scanner.ifilelen] = 0;
    ///////////////////////////////////
    import_scanner.Scan();
    const auto& stoks = import_scanner.tokens;
    //auto& dtoks       = scanner.tokens;
    //dtoks.insert(dtoks.begin() + itokidx, stoks.begin(), stoks.end());
  }
}

void ConfigNode::generate(Container* c) const {
  Config* pcfg = new Config;
  pcfg->mName  = _name;
  c->addConfig(pcfg);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////
