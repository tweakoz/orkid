#pragma once

#include <ork/util/scanner.h>

namespace ork {

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
    return _input_view.token(i);
  }
  const ScannerView& _input_view;
  size_t _start = -1;
  size_t _end   = -1;
};
using scannerlightview_ptr_t      = std::shared_ptr<ScannerLightView>;
using scannerlightview_constptr_t = std::shared_ptr<const ScannerLightView>;
using matcher_fn_t                = std::function<scannerlightview_ptr_t(scannerlightview_constptr_t& inp_view)>;

//////////////////////////////////////////////////////////////

struct Matcher {
  inline Matcher(matcher_fn_t match_fn)
      : _match_fn(match_fn) {
  }
  inline scannerlightview_ptr_t match(scannerlightview_constptr_t inp_view) const {
    return _match_fn(inp_view);
  }
  matcher_fn_t _match_fn;
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

  std::unordered_set<matcher_ptr_t> _matchers;

};

//////////////////////////////////////////////////////////////////////

inline matcher_ptr_t Parser::optional(matcher_ptr_t matcher) {
  return nullptr;
}

//////////////////////////////////////////////////////////////////////

inline matcher_ptr_t Parser::sequence(std::vector<matcher_ptr_t> matchers) {
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

#define MATCHER(x) auto x = createMatcher([=](scannerlightview_constptr_t inp_view)->scannerlightview_ptr_t


}