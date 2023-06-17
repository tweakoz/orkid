#pragma once

#include <ork/orkstd.h>
#include <ork/util/scanner.h>
#include <unordered_set>
#include <unordered_map>

namespace ork {

struct Matcher;
struct Parser;

using matcher_ptr_t               = std::shared_ptr<Matcher>;
using matcher_fn_t                = std::function<scannerlightview_ptr_t(scannerlightview_constptr_t& inp_view)>;
using matcher_notif_t             = std::function<void(scannerlightview_ptr_t)>;
using parser_ptr_t                = std::shared_ptr<Parser>;

//////////////////////////////////////////////////////////////

struct Matcher {
  Matcher(matcher_fn_t match_fn);
  scannerlightview_ptr_t match(Parser* parser, scannerlightview_constptr_t inp_view) const;

  matcher_fn_t _match_fn;
  matcher_notif_t _notif;
  std::string _name;
};

//////////////////////////////////////////////////////////////

struct Parser {

  matcher_ptr_t sequence(std::vector<matcher_ptr_t> matchers, std::string name="");
  matcher_ptr_t sequence(std::string name, std::vector<matcher_ptr_t> matchers);
  //
  matcher_ptr_t group(std::vector<matcher_ptr_t> matchers,std::string name="");
  matcher_ptr_t oneOf(std::vector<matcher_ptr_t> matchers,std::string name="");
  matcher_ptr_t oneOrMore(matcher_ptr_t matcher,std::string name="");
  matcher_ptr_t zeroOrMore(matcher_ptr_t matcher,std::string name="");
  matcher_ptr_t optional(matcher_ptr_t matcher,std::string name="");
  //
  matcher_ptr_t createMatcher(matcher_fn_t match_fn,std::string name="");
  matcher_ptr_t matcherForTokenClassID(uint64_t tokclass,std::string name="");
  matcher_ptr_t matcherForWord(std::string word);

  template <typename T> matcher_ptr_t matcherForTokenClass(T tokclass,std::string name="") {
    return matcherForTokenClassID(uint64_t(tokclass),name);
  }

  bool match(scannerlightview_constptr_t inp_view, matcher_ptr_t top);

  std::stack<const Matcher*> _matcherstack;
  std::unordered_set<matcher_ptr_t> _matchers;
};

//////////////////////////////////////////////////////////////

} // namespace ork