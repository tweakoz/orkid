#pragma once

#include <ork/orkstd.h>
#include <ork/kernel/svariant.h>
#include <ork/util/scanner.h>
#include <unordered_set>
#include <unordered_map>

namespace ork {

struct Match;
struct Sequence;
struct Group;
struct Matcher;
struct Parser;
struct OneOrMore;
struct ZeroOrMore;

using match_ptr_t                 = std::shared_ptr<Match>;
using matcher_ptr_t               = std::shared_ptr<Matcher>;
using matcher_fn_t                = std::function<match_ptr_t(matcher_ptr_t par_matcher,scannerlightview_constptr_t& inp_view)>;
using matcher_notif_t             = std::function<void(match_ptr_t)>;
using parser_ptr_t                = std::shared_ptr<Parser>;

using sequence_ptr_t              = std::shared_ptr<Sequence>;
using group_ptr_t                 = std::shared_ptr<Group>;
using one_or_more_ptr_t           = std::shared_ptr<OneOrMore>;
using zero_or_more_ptr_t           = std::shared_ptr<ZeroOrMore>;

//////////////////////////////////////////////////////////////

struct Match {
  matcher_ptr_t _matcher;
  scannerlightview_ptr_t _view;
  svar32_t _impl;
  void dump(int indent) const;
};

struct Matcher {
  Matcher(matcher_fn_t match_fn);
  matcher_fn_t _match_fn;
  matcher_notif_t _notif;
  std::string _name;
};

//////////////////////////////////////////////////////////////

struct Sequence{
  std::vector<match_ptr_t> _items;
};
struct Group{
  std::vector<match_ptr_t> _items;
};
struct OneOrMore{
  std::vector<match_ptr_t> _items;
};
struct ZeroOrMore{
  std::vector<match_ptr_t> _items;
};

//////////////////////////////////////////////////////////////

struct Parser {

  matcher_ptr_t declare(std::string name);

  matcher_ptr_t sequence(std::vector<matcher_ptr_t> matchers, std::string name="");
  matcher_ptr_t sequence(std::string name, std::vector<matcher_ptr_t> matchers);
  //
  matcher_ptr_t oneOf(std::vector<matcher_ptr_t> matchers,std::string name="");
  matcher_ptr_t oneOf(std::string name,std::vector<matcher_ptr_t> matchers);
  //
  matcher_ptr_t group(std::vector<matcher_ptr_t> matchers,std::string name="");
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

  match_ptr_t match(scannerlightview_constptr_t inp_view, matcher_ptr_t top);
  match_ptr_t _match(matcher_ptr_t matcher, scannerlightview_constptr_t inp_view);

  std::stack<matcher_ptr_t> _matcherstack;
  std::stack<const Match*> _matchstack;
  std::unordered_set<matcher_ptr_t> _matchers;
};

//////////////////////////////////////////////////////////////

} // namespace ork