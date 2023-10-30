////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
namespace ork::lev2::glslfx::parser {
/////////////////////////////////////////////////////////////////////////////////////////////////

void ConfigNode::parse(GlSlFxParser* parser, const ScannerView& view) {
  NamedBlockNode::parse(parser,view);
  auto topnode = parser->_topNode;
  ///////////////////////////////////
  int ist = view._start + 1;
  int ien = view._end - 1;
  std::vector<std::string> config_block_imports;
  for (size_t i = ist; i <= ien;) {
    const Token* vt_tok = view.token(i);
    if (vt_tok->text == "import") {
      const Token* impnam = view.token(i + 1);
      std::string p       = impnam->text.substr(1, impnam->text.length() - 2);
      config_block_imports.push_back(p);
      i += 3;
    } else
      i++;
  }
  ///////////////////////////////////
  // handle imports (deprecated)
  ///////////////////////////////////
  for (auto imp_path : config_block_imports) {

    auto import = std::make_shared<ImportNode>(imp_path,topnode.get());
    import->load();
    topnode->_imports.push_back(import);
  }
}

void ConfigNode::generate(rootcontainer_ptr_t c) const {
  Config* pcfg = new Config;
  pcfg->mName  = _name;
  c->addConfig(pcfg);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
/////////////////////////////////////////////////////////////////////////////////////////////////
