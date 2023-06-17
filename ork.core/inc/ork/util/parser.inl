#pragma once

#include <ork/orkstd.h>
#include <ork/util/scanner.h>
#include <unordered_set>
#include <unordered_map>

namespace ork {

struct Matcher;

struct ScannerLightView {
  inline ScannerLightView(const ScannerView& inp_view)
      : _input_view(inp_view)
      , _start(inp_view._start)
      , _end(inp_view._end) {
  }
  inline ScannerLightView(const ScannerLightView& oth)
      : _input_view(oth._input_view)
      , _start(oth._start)
      , _end(oth._end) {
  }
  inline const Token* token(size_t i) const {
    return _input_view.token(i + _start);
  }
  void dump(const std::string& dumpid) const {
    printf("ScannerLightView<%p>::Dump(id:%s)\n", (void*)this, dumpid.c_str());

    printf(" _start<%d>\n", int(_start));
    printf(" _end<%d>\n", int(_end));

    int i = 0;
    for (int tokidx = _start; tokidx <= _end; ++tokidx) {
      auto t = _input_view.token(tokidx);
      printf("tok<%d> val<%s>\n", tokidx, t->text.c_str());
    }
  }
  const ScannerView& _input_view;
  size_t _start = -1;
  size_t _end   = -1;
};
using scannerlightview_ptr_t      = std::shared_ptr<ScannerLightView>;
using scannerlightview_constptr_t = std::shared_ptr<const ScannerLightView>;
using matcher_fn_t                = std::function<scannerlightview_ptr_t(scannerlightview_constptr_t& inp_view)>;
using matcher_notif_t             = std::function<void(scannerlightview_ptr_t)>;
//////////////////////////////////////////////////////////////

struct Matcher {
  inline Matcher(matcher_fn_t match_fn)
      : _match_fn(match_fn) {
  }
  inline scannerlightview_ptr_t match(scannerlightview_constptr_t inp_view) const {
    auto slv = _match_fn(inp_view);
    if (slv and _notif) {
      _notif(slv);
    }
    return slv;
  }
  matcher_fn_t _match_fn;
  matcher_notif_t _notif;
};

using matcher_ptr_t = std::shared_ptr<Matcher>;

struct Parser {

  matcher_ptr_t sequence(std::vector<matcher_ptr_t> matchers);
  matcher_ptr_t group(std::vector<matcher_ptr_t> matchers);
  matcher_ptr_t oneOf(std::vector<matcher_ptr_t> matchers);
  matcher_ptr_t oneOrMore(matcher_ptr_t matcher);
  matcher_ptr_t zeroOrMore(matcher_ptr_t matcher);
  matcher_ptr_t optional(matcher_ptr_t matcher);
  //
  matcher_ptr_t createMatcher(matcher_fn_t match_fn);
  matcher_ptr_t matcherForTokenClassID(uint64_t tokclass);
  matcher_ptr_t matcherForWord(std::string word);

  template <typename T> matcher_ptr_t matcherForTokenClass(T tokclass) {
    return matcherForTokenClassID(uint64_t(tokclass));
  }

  std::unordered_set<matcher_ptr_t> _matchers;
};

using parser_ptr_t = std::shared_ptr<Parser>;

//////////////////////////////////////////////////////////////////////

inline matcher_ptr_t Parser::optional(matcher_ptr_t matcher) {
  return nullptr;
}

//////////////////////////////////////////////////////////////////////

inline matcher_ptr_t Parser::sequence(std::vector<matcher_ptr_t> matchers) {
  auto match_fn = [matchers](scannerlightview_constptr_t slv) -> scannerlightview_ptr_t {
    auto slv_temp  = std::make_shared<ScannerLightView>(*slv);
    auto slv_match = std::make_shared<ScannerLightView>(*slv);
    //slv->dump("slv");
    for (auto m : matchers) {
      auto slv_match_item = m->match(slv_temp);
      if (slv_match_item) {
        //slv_match_item->dump("slv_match_item");
        slv_match->_end  = slv_match_item->_end;
        slv_temp->_start = slv_match_item->_end + 1;
        //slv_temp->dump("slv_temp");
      } else {
        return nullptr;
      }
    }
    return slv_match;
  };
  return createMatcher(match_fn);
}

//////////////////////////////////////////////////////////////////////

inline matcher_ptr_t Parser::group(std::vector<matcher_ptr_t> matchers) {
  auto match_fn = [matchers](scannerlightview_constptr_t slv) -> scannerlightview_ptr_t {
    auto slv_test = std::make_shared<ScannerLightView>(*slv);
    std::vector<scannerlightview_ptr_t> items;
    for (auto m : matchers) {
      auto slv_match = m->match(slv_test);
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
  return createMatcher(match_fn);
}

//////////////////////////////////////////////////////////////////////

inline matcher_ptr_t Parser::oneOf(std::vector<matcher_ptr_t> matchers) {
  auto match_fn = [matchers](scannerlightview_constptr_t slv) -> scannerlightview_ptr_t {
    for (auto m : matchers) {
      auto slv_match = m->match(slv);
      if (slv_match) {
        return slv_match;
      }
    }
    return nullptr;
  };
  return createMatcher(match_fn);
}

//////////////////////////////////////////////////////////////////////

inline matcher_ptr_t Parser::oneOrMore(matcher_ptr_t matcher) {
  auto match_fn = [matcher](scannerlightview_constptr_t input_slv) -> scannerlightview_ptr_t {
    std::vector<scannerlightview_ptr_t> items;
    auto slv_out = std::make_shared<ScannerLightView>(*input_slv);
    while (slv_out) {
      if (slv_out) {
        items.push_back(slv_out);
      }
      slv_out = matcher->match(slv_out);
    }
    if (items.size()) {
      auto slv_out    = std::make_shared<ScannerLightView>(*input_slv);
      slv_out->_start = items.front()->_start;
      slv_out->_end   = items.back()->_end;
      return slv_out;
    }
    return nullptr;
  };
  return createMatcher(match_fn);
}

//////////////////////////////////////////////////////////////////////

inline matcher_ptr_t Parser::zeroOrMore(matcher_ptr_t matcher) {
  auto match_fn = [matcher](scannerlightview_constptr_t input_slv) -> scannerlightview_ptr_t {
    std::vector<scannerlightview_ptr_t> items;
    auto slvtest = std::make_shared<ScannerLightView>(*input_slv);
    while (slvtest) {
      auto slv_matched = matcher->match(slvtest);
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
    return slv_out;
  };
  return createMatcher(match_fn);
}

//////////////////////////////////////////////////////////////////////

inline matcher_ptr_t Parser::createMatcher(matcher_fn_t match_fn) {
  auto matcher = std::make_shared<Matcher>(match_fn);
  _matchers.insert(matcher);
  return matcher;
}

//////////////////////////////////////////////////////////////

inline matcher_ptr_t Parser::matcherForTokenClassID(uint64_t tokclass) {
  auto match_fn = [tokclass](scannerlightview_constptr_t slv) -> scannerlightview_ptr_t {
    auto slv_tokclass = slv->token(0)->_class;
    if (slv_tokclass == tokclass) {
      auto slv_out = std::make_shared<ScannerLightView>(*slv);
      slv_out->_end = slv_out->_start;
      return slv_out;
    }
    return nullptr;
  };
  return createMatcher(match_fn);
}

//////////////////////////////////////////////////////////////

inline matcher_ptr_t Parser::matcherForWord(std::string word) {
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
  return createMatcher(match_fn);
}

//////////////////////////////////////////////////////////////

} // namespace ork