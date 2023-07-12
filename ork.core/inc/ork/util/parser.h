#pragma once

#include <ork/orktypes.h>
#include <ork/orkstd.h>
#include <ork/kernel/varmap.inl>
#include <ork/util/scanner.h>
#include <unordered_set>
#include <unordered_map>
#include <ork/util/crc.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

struct Match;
struct MatchContext;
struct Sequence;
struct Group;
struct Matcher;
struct Parser;
struct NOrMore;
struct Optional;
struct WordMatch;
struct ClassMatch;
struct OneOf;

///////////////////////////////////////////////////////////////////////////////

using match_ptr_t     = std::shared_ptr<Match>;
using matcher_ptr_t   = std::shared_ptr<Matcher>;
using matcher_fn_t    = std::function<match_ptr_t(matcher_ptr_t par_matcher, scannerlightview_constptr_t& inp_view)>;
using matcher_notif_t = std::function<void(match_ptr_t)>;
using parser_ptr_t    = std::shared_ptr<Parser>;
using sequence_ptr_t   = std::shared_ptr<Sequence>;
using group_ptr_t      = std::shared_ptr<Group>;
using n_or_more_ptr_t  = std::shared_ptr<NOrMore>;
using oneof_ptr_t      = std::shared_ptr<OneOf>;
using optional_ptr_t   = std::shared_ptr<Optional>;
using wordmatch_ptr_t  = std::shared_ptr<WordMatch>;
using classmatch_ptr_t = std::shared_ptr<ClassMatch>;
using matchctx_ptr_t   = std::shared_ptr<MatchContext>;

using matcher_filterfn_t = std::function<bool(match_ptr_t top_match)>;
using matcher_pair_t     = std::pair<std::string, matcher_ptr_t>;

//////////////////////////////////////////////////////////////

struct Match {
  match_ptr_t _parent;
  std::vector<match_ptr_t> _children;
  matcher_ptr_t _matcher;
  scannerlightview_ptr_t _view;
  std::vector<matcher_ptr_t> _matcherstack;
  svar32_t _impl;
  svar32_t _impl2;
  varmap::VarMap _uservars;
  void dump1(int indent) const;
  void dump2(int indent) const;
  using visit_fn_t = std::function<void(int, const Match*)>;
  void visit(int level, visit_fn_t) const;
  bool matcherInStack(matcher_ptr_t matcher) const;
  template <typename impl_t> std::shared_ptr<impl_t> asShared() {
    return _impl.getShared<impl_t>();
  }
  template <typename impl_t> attempt_cast<std::shared_ptr<impl_t>> tryAsShared() {
    return _impl.tryAsShared<impl_t>();
  }
  template <typename impl_t> attempt_cast<impl_t> tryAs() {
    return _impl.tryAs<impl_t>();
  }
  template <typename impl_t> attempt_cast_const<impl_t> tryAs() const {
    return _impl.tryAs<impl_t>();
  }
  template <typename impl_t> bool isShared() const {
    return _impl.isShared<impl_t>();
  }
  template <typename user_t> std::shared_ptr<user_t> sharedForKey(std::string named) {
    using ptr_t = std::shared_ptr<user_t>;
    return _uservars.typedValueForKey<ptr_t>(named).value();
  }
  template <typename user_t> std::shared_ptr<user_t> makeSharedForKey(std::string named) {
    return _uservars.makeSharedForKey<user_t>(named);
  }
  template <typename user_t> void setSharedForKey(std::string named,std::shared_ptr<user_t> ptr) {
    return _uservars.set<std::shared_ptr<user_t>>(named);
  }
};

///////////////////////////////////////////////////////////////////////////////

struct Matcher {
  Matcher(matcher_fn_t match_fn);
  matcher_fn_t _match_fn;
  matcher_filterfn_t _match_filter;
  matcher_notif_t _notif;
  std::string _name;
  std::string _info;
  matcher_ptr_t _proxy_target;
  std::function<bool()> _on_link;
  varmap::VarMap _uservars;
  Matcher* resolve() {
    return _proxy_target ? _proxy_target.get() : this;
  }
  uint64_t hash(scannerlightview_constptr_t slv) const; // packrat hash
  void _hash(boost::Crc64& crc_out) const;              // packrat hash
};

///////////////////////////////////////////////////////////////////////////////

struct MatchContextItem {
  matcher_ptr_t _matcher;
  scannerlightview_constptr_t _view;
};

///////////////////////////////////////////////////////////////////////////////

struct MatchContext {
  matcher_ptr_t _topmatcher;
  scannerlightview_constptr_t _topview;
  std::vector<MatchContextItem> _stack;
};

//////////////////////////////////////////////////////////////

struct Sequence {
  template <typename impl_t> std::shared_ptr<impl_t> itemAsShared(int index) {
    return _items[index]->asShared<impl_t>();
  }
  std::vector<match_ptr_t> _items;
};
struct Group {
  template <typename impl_t> std::shared_ptr<impl_t> itemAsShared(int index) {
    return _items[index]->asShared<impl_t>();
  }
  std::vector<match_ptr_t> _items;
};
struct NOrMore {
  template <typename impl_t> std::shared_ptr<impl_t> itemAsShared(int index) {
    return _items[index]->asShared<impl_t>();
  }
  std::vector<match_ptr_t> _items;
  size_t _minmatches   = 0;
  bool _mustConsumeAll = false;
};
struct Optional {
  template <typename impl_t> std::shared_ptr<impl_t> asShared() {
    return _subitem->asShared<impl_t>();
  }
  match_ptr_t _subitem;
};
struct WordMatch {
  const Token* _token = nullptr;
};
struct ClassMatch {
  uint64_t _tokclass  = 0;
  const Token* _token = nullptr;
};
struct OneOf {
  template <typename impl_t> std::shared_ptr<impl_t> asShared() {
    return _selected->asShared<impl_t>();
  }
  match_ptr_t _selected;
};

//////////////////////////////////////////////////////////////

struct Parser {

  Parser();

  matcher_ptr_t declare(std::string name);

  matcher_ptr_t sequence(std::vector<matcher_ptr_t> matchers, std::string name = "");
  matcher_ptr_t sequence(std::string name, std::vector<matcher_ptr_t> sub_matchers);
  void _sequence(matcher_ptr_t matcher, std::vector<matcher_ptr_t> sub_matchers);
  //
  matcher_ptr_t oneOf(std::vector<matcher_ptr_t> sub_matchers, std::string name = "");
  matcher_ptr_t oneOf(std::string name, std::vector<matcher_ptr_t> sub_matchers);
  //
  matcher_ptr_t group(std::vector<matcher_ptr_t> sub_matchers, std::string name = "");
  matcher_ptr_t oneOrMore(matcher_ptr_t matcher, std::string name = "");
  matcher_ptr_t zeroOrMore(matcher_ptr_t matcher, std::string name = "", bool mustConsumeAll = false);
  matcher_ptr_t nOrMore(matcher_ptr_t sub_matcher, size_t minMatches, std::string name = "", bool mustConsumeAll = false);
  matcher_ptr_t optional(matcher_ptr_t matcher, std::string name = "");

  matcher_ptr_t matcherForTokenClassID(uint64_t tokclass, std::string name = "");
  matcher_ptr_t matcherForWord(std::string word, std::string name = "");

  matcher_ptr_t findMatcherByName(const std::string& name) const;

  template <typename T> matcher_ptr_t matcherForTokenClass(T tokclass, std::string name = "") {
    return matcherForTokenClassID(uint64_t(tokclass), name);
  }

  match_ptr_t match(matcher_ptr_t topmatcher, scannerlightview_constptr_t topview);
  match_ptr_t _match(MatchContextItem& mci);

  void _log_valist(const char* pMsgFormat, va_list args) const;
  void _log_valist_continue(const char* pMsgFormat, va_list args) const;
  void _log_valist_begin(const char* pMsgFormat, va_list args) const;

  void log_info(const char* pMsgFormat, ...) const;
  void log_info_begin(const char* pMsgFormat, ...) const;
  void log_info_continue(const char* pMsgFormat, ...) const;

  void log_match(const char* pMsgFormat, ...) const;
  void log_match_begin(const char* pMsgFormat, ...) const;
  void log_match_continue(const char* pMsgFormat, ...) const;

  matcher_ptr_t rule(const std::string& rule_name);
  void on(const std::string& rule_name, matcher_notif_t fn);

  match_ptr_t loadPEGScannerSpec(const std::string& spec);
  match_ptr_t loadPEGParserSpec(const std::string& spec);

  void link();
  match_ptr_t pushMatch();
  match_ptr_t leafMatch();
  void popMatch();

  std::unordered_set<matcher_ptr_t> _matchers;
  std::unordered_map<std::string, matcher_ptr_t> _matchers_by_name;
  std::unordered_map<uint64_t, match_ptr_t> _packrat_cache;
  std::vector<match_ptr_t> _match_stack;

  scanner_ptr_t _scanner;
  svar64_t _user;
  size_t _cache_misses = 0;
  size_t _cache_hits   = 0;
  bool _DEBUG_MATCH    = false;
  bool _DEBUG_INFO     = false;
  std::string _name;
  MatchContext _matchctx;
};

//////////////////////////////////////////////////////////////

} // namespace ork