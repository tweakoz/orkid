////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <regex>
#include <stdlib.h>
#include <stdarg.h>
#include <ork/pch.h>
#include <ork/file/file.h>
#include <ork/util/parser.h>
#include <ork/util/logger.h>
#include <ork/kernel/string/deco.inl>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork {
//////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_parser = logger()->createChannel("RULESPEC", fvec3(0.5, 0.7, 0.5), true);

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

void MatchAttempt::dump1(int indent) {

  auto indentstr = std::string(indent, ' ');

  if (_view->empty()) {
    logchan_parser->log(
        "%s DUMP Match<%p> matcher<%p:%s> view (empty)", indentstr.c_str(), this, (void*)_matcher.get(), _matcher->_name.c_str());
  } else {
    logchan_parser->log(
        "%s DUMP Match<%p> matcher<%p:%s> view [%zu..%zu]",
        indentstr.c_str(),
        this,
        (void*)_matcher.get(),
        _matcher->_name.c_str(),
        _view->_start,
        _view->_end);
  }

  if (auto as_seq = tryAsShared<SequenceAttempt>()) {
    auto seq = as_seq.value();
    logchan_parser->log("%s   SEQ<%p>", indentstr.c_str(), (void*)seq.get());
    for (auto i : seq->_items) {
      i->dump1(indent + 3);
    }
  } else if (auto as_nom = tryAsShared<NOrMoreAttempt>()) {
    auto nom = as_nom.value();
    logchan_parser->log("%s   NOM%zu<%p>", indentstr.c_str(), nom->_minmatches, (void*)nom.get());
    for (auto i : nom->_items) {
      i->dump1(indent + 3);
    }
  } else if (auto as_grp = tryAsShared<GroupAttempt>()) {
    auto grp = as_grp.value();
    logchan_parser->log("%s   GRP<%p>", indentstr.c_str(), (void*)grp.get());
    for (auto i : grp->_items) {
      i->dump1(indent + 3);
    }
  } else if (auto as_opt = tryAsShared<OptionalAttempt>()) {
    auto opt = as_opt.value();
    logchan_parser->log("%s   OPT<%p>", indentstr.c_str(), (void*)opt.get());
    if (opt->_subitem)
      opt->_subitem->dump1(indent + 3);
    else {
      logchan_parser->log("%s     EMPTY", indentstr.c_str());
    }
  }
}

//////////////////////////////////////////////////////////////////////

void MatchAttempt::dump2(int indent) {
  auto indentstr = std::string(indent*2, ' ');
  std::string name = "anon";
  if( _matcher ){
    name = _matcher->_name;
  }
  logchan_parser->log("%d %s   match<%p> matcher<%s>", indent,indentstr.c_str(), (void*) this, name.c_str() );
  for( auto c : _children ){
    c->dump2(indent+1);
  }
}

//////////////////////////////////////////////////////////////////////

match_ptr_t MatchAttempt::genmatch(match_attempt_constptr_t attempt){
  OrkAssert(attempt);
  OrkAssert(attempt->_matcher);
  auto fn = attempt->_matcher->_genmatch_fn;
  OrkAssert(fn);
  if (fn== nullptr) {
    logerrchannel()->log("matcher<%s> has no _genmatch_fn function", attempt->_matcher->_name.c_str());
    OrkAssert(false);
  }
  return fn(attempt);
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

void Match::visit(int level, visit_fn_t vfn) const {
  vfn(level,this);
  for( auto c : _children ){
    c->visit(level+1,vfn);
  }
}

//////////////////////////////////////////////////////////////////////

bool Match::matcherInStack(matcher_ptr_t matcher) const{
  bool rval = false;
  for( auto m : _matcherstack ){
    if( m == matcher ){
      rval = true;
      break;
    }
  }
  return rval;
}

//////////////////////////////////////////////////////////////////////

Match::Match(match_attempt_constptr_t attempt)
    : _attempt(attempt)
    , _matcher(attempt->_matcher)
    , _view(attempt->_view)
    , _terminal(attempt->_terminal){
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

Matcher::Matcher(matcher_fn_t match_fn)
    : _attempt_match_fn(match_fn) {
}

//////////////////////////////////////////////////////////////////////

uint64_t Matcher::hash(scannerlightview_constptr_t slv) const { // packrat support
  boost::Crc64 the_crc;
  the_crc.init();
  the_crc.accumulateItem<uint64_t>(slv->_start);
  _hash(the_crc);
  the_crc.finish();
  return the_crc.result();
}

//////////////////////////////////////////////////////////////////////

void Matcher::_hash(boost::Crc64& crc_out) const { // packrat support
  crc_out.accumulateItem<uint64_t>((uint64_t)this);
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

matcher_ptr_t Parser::declare(std::string name) {
  log_info_begin("DECLARE MATCHER<%s> ", name.c_str());
  auto it = _matchers_by_name.find(name);
  if (it != _matchers_by_name.end()) {
    log_match_continue( "pre-exists<%p>\n", (void*)it->second.get() );
    return it->second;
  }
  ///////////////////////////////////////////////
  auto rval = std::make_shared<Matcher>(nullptr);
  if (name == "") {
    size_t count = _matchers_by_name.size();
    name         = FormatString("anon_%zu", count);
  }
  _matchers.insert(rval);
  rval->_name             = name;
  _matchers_by_name[name] = rval;
    log_match_continue( "new<%p>\n", (void*) rval.get() );
  ///////////////////////////////////////////////
  return rval;
}

//////////////////////////////////////////////////////////////////////

void Parser::link() {
  std::vector<matcher_ptr_t> unlinked;
  for (auto matcher : _matchers) {
    unlinked.push_back(matcher);
  }
  int bad_iters = 0;
  while(unlinked.size()){
    auto it = unlinked.begin();
    unlinked.erase(it);
    auto matcher = *it;
    auto matcher_name = matcher->_name;
    bool OK = true;
    if (matcher->_on_link) {
      OK = matcher->_on_link();
    }
    if( OK ){
      log_info("MATCHER<%s> LINKED...", matcher_name.c_str());
    }
    else{
      log_info("MATCHER<%s> LINK FAILED, will reattempt..", matcher_name.c_str());
      unlinked.push_back(matcher);
      bad_iters++;
    }
    OrkAssert(bad_iters<1000);
  }
}

//////////////////////////////////////////////////////////////////////

match_attempt_ptr_t Parser::leafMatch(matcher_ptr_t matcher){

  match_attempt_ptr_t parent;
  if(not  _match_stack.empty() ){
    parent = _match_stack.back();
  }
  auto rval = std::make_shared<MatchAttempt>();
  rval->_parent = parent;
  rval->_matcher = matcher;
  if(parent){
    parent->_children.push_back(rval);
    printf( "xxx : LEAFMATCH parent<%p:%s> rval<%p:%s>\n", (void*)parent.get(), parent->_matcher->_name.c_str(), (void*)rval.get(), matcher->_name.c_str() );
  }
  else{
    printf( "xxx : LEAFMATCH noparent rval<%p:%s>\n", (void*)rval.get(), matcher->_name.c_str() );
  }
  rval->_terminal = true;
  return rval;
}

//////////////////////////////////////////////////////////////////////

match_attempt_ptr_t Parser::pushMatch(matcher_ptr_t matcher){

  match_attempt_ptr_t parent;
  if(not  _match_stack.empty() ){
    parent = _match_stack.back();
  }

  //////////////////////////////////////////////////////

  auto rval = std::make_shared<MatchAttempt>();
  rval->_matcher = matcher;
  rval->_parent = parent;
  if(parent){
    parent->_children.push_back(rval);
    printf( "xxx : PUSHMATCH parent<%p:%s> rval<%p:%s>\n", (void*)parent.get(), parent->_matcher->_name.c_str(), (void*)rval.get(), matcher->_name.c_str() );
  }
  else{
    printf( "xxx : PUSHMATCH noparent rval<%p:%s>\n", (void*)rval.get(), matcher->_name.c_str() );
  }

  //////////////////////////////////////////////////////

  rval->_terminal = false;
  _match_stack.push_back(rval);

  return rval;
}

//////////////////////////////////////////////////////////////////////

void Parser::popMatch(){
    _match_stack.pop_back();
}

//////////////////////////////////////////////////////////////////////

match_attempt_ptr_t Parser::_tryMatch(MatchAttemptContextItem& mci) {

  auto matcher  = mci._matcher;
  auto inp_view = mci._view;

  OrkAssert(matcher);
  if (matcher->_attempt_match_fn == nullptr) {
    logerrchannel()->log("matcher<%s> has no match function", matcher->_name.c_str());
    OrkAssert(false);
  }
  _matchattemptctx._stack.push_back(mci);
  inp_view->validate();
  auto match_attempt = matcher->_attempt_match_fn(matcher, inp_view);
  //////////////////////////////////
  _matchattemptctx._stack.pop_back();
  //////////////////////////////////
  return match_attempt;
}

//////////////////////////////////////////////////////////////

match_ptr_t Parser::match(matcher_ptr_t topmatcher, scannerlightview_constptr_t topview) {
  if (topmatcher == nullptr) {
    logerrchannel()->log("Parser<%p> no top match function", this);
    OrkAssert(false);
  }
  _matchattemptctx._stack.clear();
  _matchattemptctx._topmatcher = topmatcher;
  _matchattemptctx._topview    = topview;
  MatchAttemptContextItem mci { topmatcher, topview };
  auto root_match_attempt = _tryMatch(mci);

  if(root_match_attempt) {
    auto root_match = MatchAttempt::genmatch(root_match_attempt);

    /////////////////////////////////////
    // visit matchattempt tree
    /////////////////////////////////////

    printf( "xxx : #############################################################\n" );
    printf( "xxx : matchattempt tree\n" );
    printf( "xxx : #############################################################\n" );

    std::stack<match_attempt_ptr_t> ma_stack;
    std::stack<int> depth_stack;
    ma_stack.push(root_match_attempt);
    depth_stack.push(0);
    while(!ma_stack.empty()) {
      auto current_ma = ma_stack.top();
      int current_depth = depth_stack.top();
      auto indentstr = std::string(current_depth * 2, ' ');
      printf( "xxx : %s current_ma_attempt<%p:%s> numc<%zu>\n", indentstr.c_str(), (void*) current_ma.get(), current_ma->_matcher->_name.c_str(), current_ma->_children.size() );
      ma_stack.pop();
      depth_stack.pop();
      for(auto& child_ma : current_ma->_children) {
        ma_stack.push(child_ma);
        depth_stack.push(current_depth+1);
      }
    }

    /////////////////////////////////////
    // visit match tree
    /////////////////////////////////////

    printf( "xxx : #############################################################\n" );
    printf( "xxx : match tree\n" );
    printf( "xxx : #############################################################\n" );


    std::stack<match_ptr_t> dfs_stack;
    dfs_stack.push(root_match);
    depth_stack.push(0);
    while(!dfs_stack.empty()) {
      auto current_match = dfs_stack.top();
      int current_depth = depth_stack.top();
      auto indentstr = std::string(current_depth * 2, ' ');
      std::string suffix;
      if(current_match->_matcher->_pre_notif) {
        suffix += " HAS_PRE_NOTIF ";
        current_match->_matcher->_pre_notif(current_match);
      }
      if(current_match->_matcher->_post_notif) {
        suffix += " HAS_POST_NOTIF ";
      }
      printf( "xxx : %s current_match<%p:%s> numc<%zu> %s\n", indentstr.c_str(), (void*) current_match.get(), current_match->_matcher->_name.c_str(), current_match->_children.size(), suffix.c_str() );
      dfs_stack.pop();
      depth_stack.pop();
      for(auto& child_match : current_match->_children) {
        dfs_stack.push(child_match);
        depth_stack.push(current_depth+1);
      }
      if(current_match->_matcher->_post_notif) {
        current_match->_matcher->_post_notif(current_match);
      }
    }

    printf( "xxx : #############################################################\n" );


    /////////////////////////////////////
    return root_match;
  }

  return nullptr;
}

//////////////////////////////////////////////////////////////////////

void Parser::_log_valist(const char* pMsgFormat, va_list args) const {
  char buf[256];
  vsnprintf_s(buf, sizeof(buf), pMsgFormat, args);
  size_t indent  = _matchattemptctx._stack.size();
  auto indentstr = std::string(indent * 2, ' ');
  printf("[PARSER : %s] %s%s\n", _name.c_str(), indentstr.c_str(), buf);
}
void Parser::_log_valist_begin(const char* pMsgFormat, va_list args) const {
  char buf[256];
  vsnprintf_s(buf, sizeof(buf), pMsgFormat, args);
  size_t indent  = _matchattemptctx._stack.size();
  auto indentstr = std::string(indent * 2, ' ');
  printf("[PARSER] %s%s", indentstr.c_str(), buf);
}
void Parser::_log_valist_continue(const char* pMsgFormat, va_list args) const {
  char buf[256];
  vsnprintf_s(buf, sizeof(buf), pMsgFormat, args);
  printf("%s", buf);
}

//////////////////////////////////////////////////////////////////////

void Parser::log_match(const char* pMsgFormat, ...) const {
  if (_DEBUG_MATCH) {
    va_list args;
    va_start(args, pMsgFormat);
    _log_valist(pMsgFormat, args);
    va_end(args);
  }
}
void Parser::log_match_begin(const char* pMsgFormat, ...) const {
  if (_DEBUG_MATCH) {
    va_list args;
    va_start(args, pMsgFormat);
    _log_valist_begin(pMsgFormat, args);
    va_end(args);
  }
}
void Parser::log_match_continue(const char* pMsgFormat, ...) const {
  if (_DEBUG_MATCH) {
    va_list args;
    va_start(args, pMsgFormat);
    _log_valist_continue(pMsgFormat, args);
    va_end(args);
  }
}

//////////////////////////////////////////////////////////////////////

void Parser::log_info(const char* pMsgFormat, ...) const {
  if (_DEBUG_INFO) {
    va_list args;
    va_start(args, pMsgFormat);
    _log_valist(pMsgFormat, args);
    va_end(args);
  }
}
void Parser::log_info_begin(const char* pMsgFormat, ...) const {
  if (_DEBUG_INFO) {
    va_list args;
    va_start(args, pMsgFormat);
    _log_valist_begin(pMsgFormat, args);
    va_end(args);
  }
}
void Parser::log_info_continue(const char* pMsgFormat, ...) const {
  if (_DEBUG_INFO) {
    va_list args;
    va_start(args, pMsgFormat);
    _log_valist_continue(pMsgFormat, args);
    va_end(args);
  }
}

//////////////////////////////////////////////////////////////////////


Parser::Parser() {
  _name                                    = FormatString("%p", (void*)this);
  static constexpr const char* block_regex = "(function|yo|xxx)";
  _scanner                                 = std::make_shared<Scanner>(block_regex);
}

matcher_ptr_t Parser::findMatcherByName(const std::string& name) const {
  auto it = _matchers_by_name.find(name);
  if (it != _matchers_by_name.end()) {
    return it->second;
  }
  return nullptr;
}

//////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork
/////////////////////////////////////////////////////////////////////////////////////////////////
