#pragma once

#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <ork/orktypes.h>
#include <ork/orkstd.h>
#include <ork/kernel/varmap.inl>
#include <ork/kernel/treeops.inl>
#include <ork/util/scanner.h>
#include <ork/util/crc.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

struct MatchAttempt;
struct Match;
struct MatchAttemptContext;

struct SequenceAttempt;
struct GroupAttempt;
struct MatcherAttempt;
struct ParserAttempt;
struct NOrMoreAttempt;
struct OptionalAttempt;
struct WordMatchAttempt;
struct ClassMatchAttempt;
struct OneOfAttempt;

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

using match_ptr_t              = std::shared_ptr<Match>;
using match_rawptr_t           = Match*;
using match_attempt_ptr_t      = std::shared_ptr<MatchAttempt>;
using match_attempt_constptr_t = std::shared_ptr<MatchAttempt>;
using matcher_ptr_t            = std::shared_ptr<Matcher>;
using matcher_fn_t  = std::function<match_attempt_ptr_t(matcher_ptr_t par_matcher, scannerlightview_constptr_t& inp_view)>;
using match_notif_t = std::function<void(match_ptr_t)>;
using parser_ptr_t  = std::shared_ptr<Parser>;

using sequence_ptr_t   = std::shared_ptr<Sequence>;
using group_ptr_t      = std::shared_ptr<Group>;
using n_or_more_ptr_t  = std::shared_ptr<NOrMore>;
using oneof_ptr_t      = std::shared_ptr<OneOf>;
using optional_ptr_t   = std::shared_ptr<Optional>;
using wordmatch_ptr_t  = std::shared_ptr<WordMatch>;
using classmatch_ptr_t = std::shared_ptr<ClassMatch>;
using matchctx_ptr_t   = std::shared_ptr<MatchAttemptContext>;

using matcher_filterfn_t = std::function<bool(match_attempt_ptr_t top_match)>;
using matcher_pair_t     = std::pair<std::string, matcher_ptr_t>;

using genmatch_fn_t = std::function<match_ptr_t(match_attempt_ptr_t)>;

//////////////////////////////////////////////////////////////

match_attempt_ptr_t filtered_match(matcher_ptr_t matcher, match_attempt_ptr_t the_match);

//////////////////////////////////////////////////////////////

struct MatchAttempt {

  //using str_lambda_t = std::function<void(const char* fmt_str, std::va_list args)>;
  //void _dump1Impl(str_lambda_t lambda, int indent);
  using logger_t = std::function<void(const std::string&)>;

  void _dumpImpl(int indent, logger_t logger);
  void dump1(int indent);
  void dump2(int indent);
  void dumpToFile(file_ptr_t out_file, int indent);
  static match_ptr_t genmatch(match_attempt_constptr_t attempt);

  template <typename impl_t> std::shared_ptr<impl_t> asShared();
  template <typename impl_t> std::shared_ptr<impl_t> makeShared();
  template <typename impl_t> attempt_cast<std::shared_ptr<impl_t>> tryAsShared();
  template <typename impl_t> attempt_cast<impl_t> tryAs();
  template <typename impl_t> attempt_cast_const<impl_t> tryAs() const;
  template <typename impl_t> bool isShared() const;

  matcher_ptr_t _matcher;
  match_attempt_ptr_t _parent;
  std::vector<match_attempt_ptr_t> _children;
  scannerlightview_ptr_t _view;
  svar32_t _impl;
  bool _terminal = false;
};

//////////////////////////////////////////////////////////////

struct Match {

  using treeops       = tree::Ops<Match>;
  using tree_constops = tree::ConstOps<Match>;

  Match(match_attempt_constptr_t attempt);

  using visit_fn_t      = std::function<void(int, const Match*)>;
  using impl_visit_fn_t = std::function<void(match_ptr_t)>;
  using walk_fn_t       = std::function<bool(const Match*)>;

  struct ImplVisitCtx {
    size_t _depth = -1;
    impl_visit_fn_t _visitfn;
  };

  using implvisitctx_ptr_t = std::shared_ptr<ImplVisitCtx>;

  void visit(int level, visit_fn_t) const;
  void dump1(int indent) const;
  std::string ldump(int indent = 0) const;
  bool matcherInStack(matcher_ptr_t matcher) const;
  template <typename impl_t> std::shared_ptr<impl_t> asShared();
  template <typename impl_t> std::shared_ptr<impl_t> makeShared();
  template <typename impl_t> attempt_cast_const<std::shared_ptr<impl_t>> tryAsShared() const;
  template <typename impl_t> attempt_cast<impl_t> tryAs();
  template <typename impl_t> attempt_cast_const<impl_t> tryAs() const;
  template <typename impl_t> bool isShared() const;
  template <typename user_t> std::shared_ptr<user_t> sharedForKey(std::string named);
  template <typename user_t> std::shared_ptr<user_t> makeSharedForKey(std::string named);
  template <typename user_t> void setSharedForKey(std::string named, std::shared_ptr<user_t> ptr);
  template <typename impl_t> std::shared_ptr<impl_t> followImplAsShared();
  static match_ptr_t followThroughProxy(match_ptr_t start);
  match_ptr_t findFirstDescendanttWithMatcher(matcher_ptr_t mchr) const;

  bool walkDown(walk_fn_t) const;
  const Match* traverseDownPath(std::string path) const;

  static void implVisit(match_ptr_t top, implvisitctx_ptr_t);
  static size_t implDistance(match_ptr_t a, match_ptr_t b);

  match_attempt_constptr_t _attempt;
  match_ptr_t _parent;
  std::vector<match_ptr_t> _children;
  matcher_ptr_t _matcher;
  scannerlightview_ptr_t _view;
  std::vector<matcher_ptr_t> _matcherstack;
  svar32_t _impl;
  svar32_t _impl2;
  bool _terminal = false;
  varmap::VarMap _uservars;
};

///////////////////////////////////////////////////////////////////////////////

struct Matcher {
  Matcher(matcher_fn_t match_fn);
  uint64_t hash(scannerlightview_constptr_t slv) const; // packrat hash
  void _hash(boost::Crc64& crc_out) const;              // packrat hash

  matcher_fn_t _attempt_match_fn;
  genmatch_fn_t _genmatch_fn;
  matcher_filterfn_t _match_filter;
  match_notif_t _pre_notif;
  match_notif_t _post_notif;
  match_notif_t _link_notif;
  std::string _name;
  std::string _info;
  std::function<bool()> _on_link;
  varmap::VarMap _uservars;
  int _linkattempts = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct MatchAttemptContextItem {
  matcher_ptr_t _matcher;
  scannerlightview_constptr_t _view;
};

///////////////////////////////////////////////////////////////////////////////

struct MatchAttemptContext {
  matcher_ptr_t _topmatcher;
  scannerlightview_constptr_t _topview;
  std::vector<MatchAttemptContextItem> _stack;
};

//////////////////////////////////////////////////////////////
// match attempt structures
//////////////////////////////////////////////////////////////

struct SequenceAttempt {
  template <typename impl_t> std::shared_ptr<impl_t> itemAsShared(int index);
  template <typename impl_t> std::shared_ptr<impl_t> tryItemAsShared(int index);
  std::vector<match_attempt_ptr_t> _items;
};
struct GroupAttempt {
  template <typename impl_t> std::shared_ptr<impl_t> itemAsShared(int index);
  std::vector<match_attempt_ptr_t> _items;
};
struct NOrMoreAttempt {
  template <typename impl_t> std::shared_ptr<impl_t> itemAsShared(int index);
  std::vector<match_attempt_ptr_t> _items;
  size_t _minmatches   = 0;
  bool _mustConsumeAll = false;
};
struct OptionalAttempt {
  template <typename impl_t> std::shared_ptr<impl_t> asShared();
  match_attempt_ptr_t _subitem;
};
struct WordMatchAttempt {
  const Token* _token = nullptr;
};
struct ClassMatchAttempt {
  uint64_t _tokclass  = 0;
  const Token* _token = nullptr;
};
struct OneOfAttempt {
  template <typename impl_t> std::shared_ptr<impl_t> asShared();
  match_attempt_ptr_t _selected;
};
struct ProxyAttempt {
  template <typename impl_t> std::shared_ptr<impl_t> asShared();
  match_attempt_ptr_t _selected;
};

//////////////////////////////////////////////////////////////
// match structures
//////////////////////////////////////////////////////////////

struct MatchItem {
  virtual ~MatchItem() {
  }
};

struct Sequence : public MatchItem {

  void dump(std::string header) const;
  template <typename impl_t> std::shared_ptr<impl_t> itemAsShared(int index);
  template <typename impl_t> attempt_cast_const<std::shared_ptr<impl_t>> tryItemAsShared(int index) const;

  std::vector<match_ptr_t> _items;
};

struct Group : public MatchItem {
  template <typename impl_t> std::shared_ptr<impl_t> itemAsShared(int index);

  std::vector<match_ptr_t> _items;
};

struct NOrMore : public MatchItem {

  void dump(std::string header) const;
  template <typename impl_t> std::shared_ptr<impl_t> itemAsShared(int index);

  std::vector<match_ptr_t> _items;
  size_t _minmatches   = 0;
  bool _mustConsumeAll = false;
};

struct Optional : public MatchItem {
  template <typename impl_t> std::shared_ptr<impl_t> asShared();

  match_ptr_t _subitem;
};

struct WordMatch : public MatchItem {
  const Token* _token = nullptr;
};

struct ClassMatch : public MatchItem {
  uint64_t _tokclass  = 0;
  const Token* _token = nullptr;
};

struct OneOf : public MatchItem {

  void dump(std::string header) const;
  template <typename impl_t> std::shared_ptr<impl_t> asShared();

  match_ptr_t _selected;
};

struct Proxy : public MatchItem {
  template <typename impl_t> std::shared_ptr<impl_t> asShared();
  template <typename impl_t> std::shared_ptr<impl_t> followAsShared();

  match_ptr_t _selected;
};

struct MatchAttemptTrackItem {
  matcher_ptr_t _matcher;
  scannerlightview_constptr_t _view;
};

using matchattempt_trackitem_ptr_t = std::shared_ptr<MatchAttemptTrackItem>;

//////////////////////////////////////////////////////////////
// the parser
//////////////////////////////////////////////////////////////

struct Parser {

  Parser();
  virtual ~Parser() {
  }

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
  void _proxy(matcher_ptr_t par_matcher, matcher_ptr_t sub_matcher);

  matcher_ptr_t matcherForTokenClassID(uint64_t tokclass, std::string name = "");
  matcher_ptr_t matcherForWord(std::string word, std::string name = "");

  matcher_ptr_t findMatcherByName(const std::string& name) const;

  template <typename T> matcher_ptr_t matcherForTokenClass(T tokclass, std::string name = "");

  match_ptr_t match(
      matcher_ptr_t topmatcher, //
      scannerlightview_constptr_t topview,
      match_notif_t prelink_notif = nullptr);

  match_attempt_ptr_t _tryMatch(MatchAttemptContextItem& mci);

  void _log_valist(scannerlightview_constptr_t view, const char* pMsgFormat, va_list args) const;
  void _log_valist_begin(scannerlightview_constptr_t view, const char* pMsgFormat, va_list args) const;
  void _log_valist_continue(const char* pMsgFormat, va_list args) const;

  void log_info(const char* pMsgFormat, ...) const;
  void log_info_begin(const char* pMsgFormat, ...) const;
  void log_info_continue(const char* pMsgFormat, ...) const;

  void log_match( scannerlightview_constptr_t view, const char* pMsgFormat, ...) const;
  void log_match_begin(scannerlightview_constptr_t view, const char* pMsgFormat, ...) const;
  void log_match_continue(const char* pMsgFormat, ...) const;

  matcher_ptr_t rule(const std::string& rule_name);
  void onPre(const std::string& rule_name, match_notif_t fn);
  void onPost(const std::string& rule_name, match_notif_t fn);
  void onLink(const std::string& rule_name, match_notif_t fn);

  bool loadPEGSpec(
      const std::string& scanner_spec, //
      const std::string& parser_spec);

  void link();
  match_attempt_ptr_t pushAttempt(matcher_ptr_t matcher);
  match_attempt_ptr_t leafAttempt(matcher_ptr_t matcher);
  void popAttempt( match_attempt_ptr_t attempt,
                   matcher_ptr_t matcher,
                   scannerlightview_constptr_t view);

  match_ptr_t createMatch(match_attempt_ptr_t ma);

  void _visitComposeMatch(match_ptr_t m);
  void _visitLinkMatch(match_ptr_t m);

  std::unordered_set<matcher_ptr_t> _matchers;
  std::unordered_map<std::string, matcher_ptr_t> _matchers_by_name;
  std::vector<match_attempt_ptr_t> _match_stack;

  std::unordered_map<uint64_t, match_attempt_ptr_t> _packrat_cache;

  match_ptr_t _last_match;
  matchattempt_trackitem_ptr_t _trackcontig;
  size_t _track_depth;
  size_t _high_track_depth;
  match_attempt_ptr_t _attempt_prev;

  scanner_ptr_t _scanner;
  svar64_t _user;
  size_t _cache_misses = 0;
  size_t _cache_hits   = 0;
  bool _DEBUG_MATCH    = false;
  bool _DEBUG_INFO     = false;
  std::string _name;
  MatchAttemptContext _matchattemptctx;
  size_t _visit_depth = 0;
  varmap::VarMap _uservars;
};

//////////////////////////////////////////////////////////////

} // namespace ork