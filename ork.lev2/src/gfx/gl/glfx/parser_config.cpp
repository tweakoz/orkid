////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
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
  for (auto imp : imports) {
    file::Path::NameType a, b;
    file::Path imppath;
    //////////////////////////////////
    // if import file has datasource (xxx://), use that
    //  instead of inferring from container's path
    //////////////////////////////////
    file::Path(imp).Split(a, b, ':');
    if (b.length() != 0) { // use from import
      imppath = imp;
    } else { // infer from container
      _container->_path.Split(a, b, ':');
      ork::FixedString<256> fxs;
      fxs.format("%s://%s", a.c_str(), imp.c_str());
      imppath = fxs.c_str();
    }
    auto importscanner = new Scanner(block_regex);
    ///////////////////////////////////
    File fx_file(imppath.c_str(), EFM_READ);
    OrkAssert(fx_file.IsOpen());
    EFileErrCode eFileErr = fx_file.GetLength(importscanner->ifilelen);
    importscanner->resize(importscanner->ifilelen + 1);
    eFileErr                                          = fx_file.Read(importscanner->_fxbuffer.data(), importscanner->ifilelen);
    importscanner->_fxbuffer[importscanner->ifilelen] = 0;
    performScan(*importscanner);
    ///////////////////////////////////
    auto childparser = new GlSlFxParser(imppath.c_str(), *importscanner);
    _imports.push_back(childparser);

    auto childnode = childparser->_rootNode;

    for (auto n : childnode->_orderedBlockNodes) {
      // printf("IMPORT NODE<%s>\n", n->_name.c_str());
      _container->_orderedBlockNodes.push_back(n);
    }
    for (auto i : childnode->_blockNodes) {
      auto k                     = i.first;
      auto v                     = i.second;
      _container->_blockNodes[k] = v;
    }
    for (auto i : childnode->_keywords) {
      _container->_keywords.insert(i);
    }
    for (auto i : childnode->_validTypeNames) {
      _container->_validTypeNames.insert(i);
    }
  }
}

void ConfigNode::generate(Container* c) const {
  Config* pcfg = new Config;
  pcfg->mName  = _name;
  c->addConfig(pcfg);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
/////////////////////////////////////////////////////////////////////////////////////////////////
