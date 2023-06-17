////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <regex>
#include <stdlib.h>
#include <ork/pch.h>
#include <ork/file/file.h>
#include <ork/util/parser.h>
#include <ork/kernel/string/deco.inl>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork {
/////////////////////////////////////////////////////////////////////////////////////////////////

Matcher::Matcher(matcher_fn_t match_fn)
    : _match_fn(match_fn) {
}

//////////////////////////////////////////////////////////////////////

scannerlightview_ptr_t Matcher::match(Parser* p, scannerlightview_constptr_t inp_view) const {
  p->_matcherstack.push(this);
  auto slv = _match_fn(inp_view);
  if (slv and _notif) {
    _notif(slv);
  }
  p->_matcherstack.pop();
  return slv;
}

//////////////////////////////////////////////////////////////////////

matcher_ptr_t Parser::optional(matcher_ptr_t matcher,std::string name) {
  auto match_fn = [this,matcher](scannerlightview_constptr_t slv) -> scannerlightview_ptr_t {
    // if matcher matches, return the match,
    //  otherwise return "empty" match (start=end=-1)
    auto slv_match = matcher->match(this,slv);
    if (slv_match) {
      return slv_match;
    }
    auto slv_out    = std::make_shared<ScannerLightView>(*slv);
    slv_out->clear();
    return slv_out;
  };
  return createMatcher(match_fn,name);}

//////////////////////////////////////////////////////////////////////

matcher_ptr_t Parser::sequence(std::vector<matcher_ptr_t> matchers,std::string name) {
  auto match_fn = [this,matchers,name](scannerlightview_constptr_t slv) -> scannerlightview_ptr_t {
    printf( "beg_match sequence<%s> len<%zu>\n", name.c_str(), matchers.size() );
    auto slv_temp  = std::make_shared<ScannerLightView>(*slv);
    auto slv_match = std::make_shared<ScannerLightView>(*slv);
    // slv->dump("slv");
    for (auto m : matchers) {
      auto slv_match_item = m->match(this,slv_temp);
        printf( "SLV_match sequence<%s> slv_match_item<%p>\n", name.c_str(), (void*) slv_match_item.get() );
      if (slv_match_item) {
        if( not slv_match_item->empty() ){
            slv_match->_end  = slv_match_item->_end;
            slv_temp->_start = slv_match_item->_end + 1;
        }
        // slv_temp->dump("slv_temp");
      } else {
        slv_match = nullptr;
        break;
      }
    }
    printf( "end_match sequence<%s>\n", name.c_str() );
    return slv_match;
  };
  return createMatcher(match_fn,name);
}

//////////////////////////////////////////////////////////////////////

matcher_ptr_t Parser::sequence(std::string name, std::vector<matcher_ptr_t> matchers) {
    return sequence(matchers, name);
}

//////////////////////////////////////////////////////////////////////

matcher_ptr_t Parser::group(std::vector<matcher_ptr_t> matchers,std::string name) {
  auto match_fn = [this,matchers](scannerlightview_constptr_t slv) -> scannerlightview_ptr_t {
    auto slv_test = std::make_shared<ScannerLightView>(*slv);
    std::vector<scannerlightview_ptr_t> items;
    for (auto m : matchers) {
      auto slv_match = m->match(this,slv_test);
      if (slv_match) {
        items.push_back(slv_match);
        slv_test->_start = slv_match->_end + 1;
      } else {
        return nullptr;
      }
    }
    OrkAssert(items.size());
    auto slv_out    = std::make_shared<ScannerLightView>(*slv);
    slv_out->_start = items.front()->_start;
    slv_out->_end   = items.back()->_end;
    return slv_out;
  };
  return createMatcher(match_fn,name);
}

//////////////////////////////////////////////////////////////////////

matcher_ptr_t Parser::oneOf(std::vector<matcher_ptr_t> matchers,std::string name) {
  auto match_fn = [this,matchers](scannerlightview_constptr_t slv) -> scannerlightview_ptr_t {
    for (auto m : matchers) {
      auto slv_match = m->match(this,slv);
      if (slv_match) {
        return slv_match;
      }
    }
    return nullptr;
  };
  return createMatcher(match_fn,name);
}

//////////////////////////////////////////////////////////////////////

matcher_ptr_t Parser::oneOrMore(matcher_ptr_t matcher,std::string name) {
  auto match_fn = [this,matcher](scannerlightview_constptr_t input_slv) -> scannerlightview_ptr_t {
    std::vector<scannerlightview_ptr_t> items;
    auto slv_out = std::make_shared<ScannerLightView>(*input_slv);
    while (slv_out) {
      if (slv_out) {
        items.push_back(slv_out);
      }
      slv_out = matcher->match(this,slv_out);
    }
    if (items.size()) {
      auto slv_out    = std::make_shared<ScannerLightView>(*input_slv);
      slv_out->_start = items.front()->_start;
      slv_out->_end   = items.back()->_end;
      return slv_out;
    }
    return nullptr;
  };
  return createMatcher(match_fn,name);
}

//////////////////////////////////////////////////////////////////////

matcher_ptr_t Parser::zeroOrMore(matcher_ptr_t matcher,std::string name) {
  auto match_fn = [this,matcher,name](scannerlightview_constptr_t input_slv) -> scannerlightview_ptr_t {
    printf( "beg_match zeroOrMore<%s>\n", name.c_str() );
    std::vector<scannerlightview_ptr_t> items;
    auto slvtest = std::make_shared<ScannerLightView>(*input_slv);
    while (slvtest) {
      auto slv_matched = matcher->match(this,slvtest);
      if (slv_matched) {
        items.push_back(slv_matched);
        slvtest->_start = slv_matched->_end + 1;
      } else {
        slvtest = nullptr;
      }
    }

    auto slv_out = std::make_shared<ScannerLightView>(*input_slv);
    if (items.size()) {
      slv_out->_end = items.back()->_end;
    }
    else{
        slv_out->clear();
    }
    printf( "end_match zeroOrMore<%s>\n", name.c_str() );
    return slv_out;
  };
  return createMatcher(match_fn,name);
}

//////////////////////////////////////////////////////////////////////

matcher_ptr_t Parser::createMatcher(matcher_fn_t match_fn,std::string name) {
  auto matcher = std::make_shared<Matcher>(match_fn);
  _matchers.insert(matcher);
  matcher->_name = name;
  return matcher;
}

//////////////////////////////////////////////////////////////

matcher_ptr_t Parser::matcherForTokenClassID(uint64_t tokclass,std::string name) {
  auto match_fn = [tokclass](scannerlightview_constptr_t slv) -> scannerlightview_ptr_t {
    auto slv_tokclass = slv->token(0)->_class;
    if (slv_tokclass == tokclass) {
      auto slv_out  = std::make_shared<ScannerLightView>(*slv);
      slv_out->_end = slv_out->_start;
      return slv_out;
    }
    return nullptr;
  };
  auto matcher = createMatcher(match_fn,name);
  matcher->_notif = [name](scannerlightview_ptr_t slv) {
    auto tok = slv->token(0);
    printf("MATCHED tok<%s> mname<%s>\n", tok->text.c_str(), name.c_str());
  };
  return matcher;
}

//////////////////////////////////////////////////////////////

matcher_ptr_t Parser::matcherForWord(std::string word) {
  auto match_fn = [word](scannerlightview_constptr_t inp_view) -> scannerlightview_ptr_t {
    auto tok0 = inp_view->token(0);
    if (tok0->text == word) {
      auto slv  = std::make_shared<ScannerLightView>(*inp_view);
      slv->_end = slv->_start;
      return slv;
    } else {
      return nullptr;
    }
  };
  auto matcher = createMatcher(match_fn,word);
  matcher->_notif = [word](scannerlightview_ptr_t slv) {
    printf("MATCHED word<%s>\n", word.c_str());
  };
  return matcher;
}

bool Parser::match(scannerlightview_constptr_t inp_view, matcher_ptr_t top){
    auto slv = top->match(this,inp_view);
    return slv != nullptr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork
/////////////////////////////////////////////////////////////////////////////////////////////////
