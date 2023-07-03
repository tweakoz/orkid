#pragma once

#include <ork/orkstd.h>
#include <ork/kernel/svariant.h>
#include <ork/util/scanner.h>
#include <unordered_set>
#include <unordered_map>
#include <ork/util/crc.h>

namespace ork {

struct Match;
struct Sequence;
struct Group;
struct Matcher;
struct Parser;
struct NOrMore;
struct Optional;
struct WordMatch;
struct ClassMatch;
struct OneOf;

using match_ptr_t                 = std::shared_ptr<Match>;
using matcher_ptr_t               = std::shared_ptr<Matcher>;
using matcher_fn_t                = std::function<match_ptr_t(matcher_ptr_t par_matcher,scannerlightview_constptr_t& inp_view)>;
using matcher_notif_t             = std::function<void(match_ptr_t)>;
using parser_ptr_t                = std::shared_ptr<Parser>;

using sequence_ptr_t              = std::shared_ptr<Sequence>;
using group_ptr_t                 = std::shared_ptr<Group>;
using n_or_more_ptr_t             = std::shared_ptr<NOrMore>;
using oneof_ptr_t                 = std::shared_ptr<OneOf>;
using optional_ptr_t              = std::shared_ptr<Optional>;
using wordmatch_ptr_t             = std::shared_ptr<WordMatch>;
using classmatch_ptr_t            = std::shared_ptr<ClassMatch>;

//////////////////////////////////////////////////////////////

struct Match {
  matcher_ptr_t _matcher;
  scannerlightview_ptr_t _view;
  svar32_t _impl;
  svar32_t _user;
  void dump(int indent) const;
  template <typename impl_t> std::shared_ptr<impl_t> asShared(){
    return _impl.getShared<impl_t>();
  }
  template <typename impl_t> attempt_cast<std::shared_ptr<impl_t>> tryAsShared(){
    return _impl.tryAsShared<impl_t>();
  }
  template <typename impl_t> attempt_cast<impl_t> tryAs(){
    return _impl.tryAs<impl_t>();
  }
  //template <typename impl_t> attempt_cast_const<std::shared_ptr<impl_t>> tryAsShared() const {
    //return _impl.tryAsShared<impl_t>();
  //}
  template <typename impl_t> attempt_cast_const<impl_t> tryAs() const {
    return _impl.tryAs<impl_t>();
  }
  template <typename impl_t> bool isShared() const{
    return _impl.isShared<impl_t>();
  }
};

struct Matcher {
  Matcher(matcher_fn_t match_fn);
  matcher_fn_t _match_fn;
  matcher_notif_t _notif;
  std::string _name;
  uint64_t hash(scannerlightview_constptr_t slv) const; // packrat hash
  void _hash(boost::Crc64& crc_out) const; // packrat hash
};

//////////////////////////////////////////////////////////////

struct Sequence{
  template <typename impl_t> std::shared_ptr<impl_t> itemAsShared(int index){
    return _items[index]->asShared<impl_t>();
  }
  std::vector<match_ptr_t> _items;
};
struct Group{
  template <typename impl_t> std::shared_ptr<impl_t> itemAsShared(int index){
    return _items[index]->asShared<impl_t>();
  }
  std::vector<match_ptr_t> _items;
};
struct NOrMore{
  template <typename impl_t> std::shared_ptr<impl_t> itemAsShared(int index){
    return _items[index]->asShared<impl_t>();
  }
  std::vector<match_ptr_t> _items;
  size_t _minmatches = 0;
  bool _mustConsumeAll = false;
};
struct Optional{
  template <typename impl_t> std::shared_ptr<impl_t> asShared(){
    return _subitem->asShared<impl_t>();
  }
  match_ptr_t _subitem;
};
struct WordMatch{
  const Token* _token = nullptr;
};
struct ClassMatch{
  uint64_t _tokclass = 0;
  const Token* _token = nullptr;
};
struct OneOf{
  template <typename impl_t> std::shared_ptr<impl_t> asShared(){
    return _selected->asShared<impl_t>();
  }
  match_ptr_t _selected;
};

//////////////////////////////////////////////////////////////

struct Parser {

  Parser();

  matcher_ptr_t declare(std::string name);

  matcher_ptr_t sequence(std::vector<matcher_ptr_t> matchers, std::string name="");
  matcher_ptr_t sequence(std::string name, std::vector<matcher_ptr_t> sub_matchers);
  void sequence(matcher_ptr_t matcher, std::vector<matcher_ptr_t> sub_matchers);
  //
  matcher_ptr_t oneOf(std::vector<matcher_ptr_t> sub_matchers,std::string name="");
  matcher_ptr_t oneOf(std::string name,std::vector<matcher_ptr_t> sub_matchers);
  //
  matcher_ptr_t group(std::vector<matcher_ptr_t> sub_matchers,std::string name="");
  matcher_ptr_t oneOrMore(matcher_ptr_t matcher,std::string name="");
  matcher_ptr_t zeroOrMore(matcher_ptr_t matcher,std::string name="",bool mustConsumeAll=false);
  matcher_ptr_t nOrMore(matcher_ptr_t sub_matcher, size_t minMatches, std::string name="",bool mustConsumeAll=false);
  matcher_ptr_t optional(matcher_ptr_t matcher,std::string name="");
  //
  matcher_ptr_t createMatcher(matcher_fn_t match_fn,std::string name="");
  matcher_ptr_t matcherForTokenClassID(uint64_t tokclass,std::string name="");
  matcher_ptr_t matcherForWord(std::string word);

  matcher_ptr_t findMatcherByName(const std::string& name) const;

  template <typename T> matcher_ptr_t matcherForTokenClass(T tokclass,std::string name="") {
    return matcherForTokenClassID(uint64_t(tokclass),name);
  }

  match_ptr_t match(scannerlightview_constptr_t inp_view, matcher_ptr_t top);
  match_ptr_t _match(matcher_ptr_t matcher, scannerlightview_constptr_t inp_view);

  void _log_valist(const char *pMsgFormat, va_list args) const;
  void log(const char *pMsgFormat, ...) const;

  void _log_valist_continue(const char *pMsgFormat, va_list args) const;
  void log_continue(const char *pMsgFormat, ...) const;

  void _log_valist_begin(const char *pMsgFormat, va_list args) const;
  void log_begin(const char *pMsgFormat, ...) const;

  matcher_ptr_t rule(const std::string& rule_name);
  void on(const std::string& rule_name, matcher_notif_t fn);

  void loadScannerSpec(const std::string& spec);
  void loadParserSpec(const std::string& spec);



  std::stack<matcher_ptr_t> _matcherstack;
  std::stack<const Match*> _matchstack;
  std::unordered_set<matcher_ptr_t> _matchers;
  std::unordered_map<std::string,matcher_ptr_t> _matchers_by_name;
  std::unordered_map<uint64_t,match_ptr_t> _packrat_cache;

  scanner_ptr_t _scanner;
  svar64_t _user;
  size_t _cache_misses = 0;
  size_t _cache_hits = 0;
};

//////////////////////////////////////////////////////////////

} // namespace ork