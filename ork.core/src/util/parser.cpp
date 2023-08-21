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
#include <ork/util/parser.inl>
#include <ork/util/logger.h>
#include <ork/kernel/string/deco.inl>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork {
//////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_parser = logger()->createChannel("RULESPEC", fvec3(0.5, 0.7, 0.5), false);
static logchannel_ptr_t logchan_dump   = logger()->createChannel("PARSER-DUMP", fvec3(0.5, 0.7, 0.5), true);
static logchannel_ptr_t logchan_matchattempt   = logger()->createChannel("MATCHATTEMPT", fvec3(0.5, 0.7, 0.5), true);

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

void MatchAttempt::dump1(int indent) {

  auto indentstr = std::string(indent, ' ');

  std::string log_str;

  if (_view->empty()) {
    log_str += FormatString("[e] matcher<%p:%s>) ",
        indentstr.c_str(),
        (void*)_matcher.get(),
        _matcher->_name.c_str());
  } else {
        log_str += FormatString("[%zu..%zu] matcher<%p:%s> ",
        indentstr.c_str(),
        _view->_start,
        _view->_end,
        (void*)_matcher.get(),
        _matcher->_name.c_str());
  }

  if (auto as_seq = tryAsShared<SequenceAttempt>()) {
    auto seq = as_seq.value();
    log_str += FormatString("SEQ<%p>", (void*)seq.get());
    logchan_matchattempt->log("%s", log_str.c_str());
    for (auto i : seq->_items) {
      i->dump1(indent + 3);
    }
  } else if (auto as_nom = tryAsShared<NOrMoreAttempt>()) {
    auto nom = as_nom.value();
    log_str += FormatString("NOM%zu<%p>", nom->_minmatches, (void*)nom.get());
    logchan_matchattempt->log("%s", log_str.c_str());
    for (auto i : nom->_items) {
      i->dump1(indent + 3);
    }
  } else if (auto as_grp = tryAsShared<GroupAttempt>()) {
    auto grp = as_grp.value();
     log_str += FormatString("GRP<%p>", (void*)grp.get());
    logchan_matchattempt->log("%s", log_str.c_str());
    for (auto i : grp->_items) {
      i->dump1(indent + 3);
    }
  } else if (auto as_opt = tryAsShared<OptionalAttempt>()) {
    auto opt = as_opt.value();
     log_str += FormatString("OPT<%p>", (void*)opt.get());
    if (opt->_subitem){
      logchan_matchattempt->log("%s", log_str.c_str());
      opt->_subitem->dump1(indent + 3);
    }
    else {
      log_str += FormatString("OPT<EMPTY>");
      logchan_matchattempt->log("%s", log_str.c_str());
    }
  }
}

//////////////////////////////////////////////////////////////////////

void MatchAttempt::dump2(int indent) {
  auto indentstr   = std::string(indent * 2, ' ');
  std::string name = "anon";
  if (_matcher) {
    name = _matcher->_name;
  }
  logchan_dump->log("%d %s   MatchAttempt<%p> matcher<%s>", indent, indentstr.c_str(), (void*)this, name.c_str());
  for (auto c : _children) {
    c->dump2(indent + 1);
  }
}

//////////////////////////////////////////////////////////////////////

match_ptr_t MatchAttempt::genmatch(match_attempt_constptr_t attempt) {
  OrkAssert(attempt);
  OrkAssert(attempt->_matcher);
  auto fn = attempt->_matcher->_genmatch_fn;
  OrkAssert(fn);
  if (fn == nullptr) {
    logerrchannel()->log("matcher<%s> has no _genmatch_fn function", attempt->_matcher->_name.c_str());
    OrkAssert(false);
  }
  return fn(attempt);
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

void Match::visit(int level, visit_fn_t vfn) const {
  tree_constops(this).visit(level, vfn);
}
bool Match::walkDown(walk_fn_t walk_fn) const {
  return tree_constops(this).walkDown(walk_fn);
}

//////////////////////////////////////////////////////////////////////

const Match* Match::traverseDownPath(std::string path) const {
  const Match* rval = nullptr;
  std::vector<std::string> path_segments;
  SplitString(path, '/', path_segments);
  size_t current_token_index                     = 0;
  std::function<bool(const ork::Match*)> visitor = [&](const ork::Match* m) -> bool {
    if (current_token_index >= path_segments.size())
      return false;

    if (m->_matcher->_name == path_segments[current_token_index]) {
      current_token_index++;
      if (current_token_index == path_segments.size()) {
        rval = m;
        return false;
      }
      m->walkDown(visitor);
      return false;
    }
    return true;
  };

  this->walkDown(visitor);
  return rval;
}

//////////////////////////////////////////////////////////////////////

match_ptr_t Match::followThroughProxy(match_ptr_t start) {
  if (auto as_pxy = start->tryAsShared<Proxy>()) {
    auto pxy = as_pxy.value();
    if (pxy->_selected) {
      return followThroughProxy(pxy->_selected);
    } else {
      return nullptr;
    }
  } else {
    return start;
  }
}

std::string Match::ldump(int indent) const {
  auto indentstr = std::string(indent, ' ');
  std::string rval;
  auto st = _view->_start;
  auto en = _view->_end;
  if (auto as_seq = tryAsShared<Sequence>()) {
    auto seq = as_seq.value();
    rval     = FormatString(
        "%s MATCH<%s> SEQ<%p> cnt<%zu> view<%zu:%zu>\n", //
        indentstr.c_str(),                               //
        _matcher->_name.c_str(),                         //
        (void*)seq.get(),                                //
        seq->_items.size(),                              //
        st,
        en);
    for (auto i : seq->_items) {
      rval += i->ldump(indent + 1);
    }
  } else if (auto as_grp = tryAsShared<Group>()) {
    auto grp = as_grp.value();
    rval     = FormatString(
        "%s MATCH<%s> GRP<%p> cnt<%zu> view<%zu:%zu>\n", //
        indentstr.c_str(),                               //
        _matcher->_name.c_str(),                         //
        (void*)grp.get(),                                //
        grp->_items.size(),                              //
        st,
        en);
    for (auto i : grp->_items) {
      rval += i->ldump(indent + 1);
    }
  } else if (auto as_nom = tryAsShared<NOrMore>()) {
    auto nom = as_nom.value();
    rval     = FormatString(
        "%s MATCH<%s> NOM%zu<%p> cnt<%zu> view<%zu:%zu>\n", //
        indentstr.c_str(),                                  //
        _matcher->_name.c_str(),                            //
        nom->_minmatches,                                   //
        (void*)nom.get(),                                   //
        nom->_items.size(),                                 //
        st,
        en);
    for (auto i : nom->_items) {
      rval += i->ldump(indent + 1);
    }
  } else if (auto as_opt = tryAsShared<Optional>()) {
    auto opt = as_opt.value();
    if (opt->_subitem) {
      rval = FormatString(
          "%s MATCH<%s> OPT<%p> view<%zu:%zu>\n", //
          indentstr.c_str(),                      //
          _matcher->_name.c_str(),                //
          (void*)opt.get(),                       //
          st,
          en);
      rval += opt->_subitem->ldump(indent + 1);
    } else {
      rval = FormatString(
          "%s MATCH<%s> OPT<%p>.EMPTY view<%zu:%zu>\n", //
          indentstr.c_str(),                            //
          _matcher->_name.c_str(),                      //
          (void*)opt.get(),
          st,
          en);
    }
  } else if (auto as_pxy = tryAsShared<Proxy>()) {
    auto pxy = as_pxy.value();
    if (pxy->_selected) {
      rval = FormatString(
          "%s MATCH<%s> PXY<%p> view<%zu:%zu>\n", //
          indentstr.c_str(),                      //
          _matcher->_name.c_str(),                //
          (void*)pxy.get(),                       //
          st,
          en);
      rval += pxy->_selected->ldump(indent + 1);
    } else {
      rval = FormatString(
          "%s MATCH<%s> PXY<%p>.EMPTY view<%zu:%zu>\n", //
          indentstr.c_str(),                            //
          _matcher->_name.c_str(),                      //
          (void*)pxy.get(),                             //
          st,
          en);
    }
  } else if (auto as_sel = tryAsShared<OneOf>()) {
    auto oneof = as_sel.value();
    if (oneof->_selected) {
      rval = FormatString(
          "%s MATCH<%s> SEL<%p> view<%zu:%zu>\n", //
          indentstr.c_str(),                      //
          _matcher->_name.c_str(),                //
          (void*)oneof.get(),                     //
          st,
          en);
      rval += oneof->_selected->ldump(indent + 1);
    } else {
      rval = FormatString(
          "%s MATCH<%s> SEL<%p>.EMPTY view<%zu:%zu>\n", //
          indentstr.c_str(),                            //
          _matcher->_name.c_str(),                      //
          (void*)oneof.get(),                           //
          st,
          en);
    }
  } else if (auto as_cm = tryAsShared<ClassMatch>()) {
    auto classmatch = as_cm.value();
    rval            = FormatString(
        "%s MATCH<%s> CLASS<0x%zx> text<%s> view<%zu:%zu>\n", //
        indentstr.c_str(),                                    //
        _matcher->_name.c_str(),                              //
        classmatch->_tokclass,                                //
        classmatch->_token->text.c_str(),                     //
        st,
        en);
  } else if (auto as_wm = tryAsShared<WordMatch>()) {
    auto wordmatch = as_wm.value();
    rval           = FormatString(
        "%s MATCH<%s> WORD<%s> view<%zu:%zu>\n", //
        indentstr.c_str(),                       //
        _matcher->_name.c_str(),                 //
        wordmatch->_token->text.c_str(),         //
        st,
        en);
  } else {
    OrkAssert(false);
  }
  return rval;
}

//////////////////////////////////////////////////////////////////////

void Match::implVisit(match_ptr_t top, implvisitctx_ptr_t visitctx) {

  using recurse_fn_t = std::function<void(match_ptr_t)>;

  visitctx->_depth     = 0;
  recurse_fn_t recurse = [&](match_ptr_t match) {
    visitctx->_visitfn(match);
    if (auto as_seq = match->tryAsShared<Sequence>()) {
      visitctx->_depth++;
      auto seq = as_seq.value();
      for (auto i : seq->_items) {
        recurse(i);
      }
      visitctx->_depth--;
    } else if (auto as_grp = match->tryAsShared<Group>()) {
      visitctx->_depth++;
      auto grp = as_grp.value();
      for (auto i : grp->_items) {
        recurse(i);
      }
      visitctx->_depth--;
    } else if (auto as_nom = match->tryAsShared<NOrMore>()) {
      visitctx->_depth++;
      auto nom = as_nom.value();
      for (auto i : nom->_items) {
        recurse(i);
      }
      visitctx->_depth--;
    } else if (auto as_opt = match->tryAsShared<Optional>()) {
      visitctx->_depth++;
      auto opt = as_opt.value();
      if (opt->_subitem) {
        recurse(opt->_subitem);
      }
      visitctx->_depth--;
    } else if (auto as_pxy = match->tryAsShared<Proxy>()) {
      visitctx->_depth++;
      auto pxy = as_pxy.value();
      if (pxy->_selected) {
        recurse(pxy->_selected);
      }
      visitctx->_depth--;
    } else if (auto as_sel = match->tryAsShared<OneOf>()) {
      visitctx->_depth++;
      auto oneof = as_sel.value();
      if (oneof->_selected) {
        recurse(oneof->_selected);
      }
      visitctx->_depth--;
    }
  };
  recurse(top);
}

//////////////////////////////////////////////////////////////////////

size_t Match::implDistance(match_ptr_t a, match_ptr_t b) {
  auto vctx      = std::make_shared<ImplVisitCtx>();
  size_t depth   = -1;
  vctx->_visitfn = [&](match_ptr_t m) {
    if (m == b) {
      depth = vctx->_depth;
    }
  };
  implVisit(a, vctx);
  return depth;
}

//////////////////////////////////////////////////////////////////////

void Match::dump1(int indent) const {
  std::string dstr = ldump(indent);
  logchan_dump->log("%s", dstr.c_str());
}

//////////////////////////////////////////////////////////////////////

bool Match::matcherInStack(matcher_ptr_t matcher) const {
  bool rval = false;
  for (auto m : _matcherstack) {
    if (m == matcher) {
      rval = true;
      break;
    }
  }
  return rval;
}

match_ptr_t Match::findFirstDescendanttWithMatcher(matcher_ptr_t mchr) const {
  for (auto ch : _children) {
    if (ch->_matcher == mchr) {
      return ch;
    } else {
      return ch->findFirstDescendanttWithMatcher(mchr);
    }
  }
  return nullptr;
}

//////////////////////////////////////////////////////////////////////

Match::Match(match_attempt_constptr_t attempt)
    : _attempt(attempt)
    , _matcher(attempt->_matcher)
    , _view(attempt->_view)
    , _terminal(attempt->_terminal) {
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

matcher_ptr_t Parser::rule(const std::string& rule_name) {
  auto it = _matchers_by_name.find(rule_name);
  matcher_ptr_t rval;
  if (it != _matchers_by_name.end()) {
    rval = it->second;
  } else {
    printf("Rule<%s> Not Found!\n", rule_name.c_str());
    OrkAssert(false);
  }
  return rval;
}

//////////////////////////////////////////////////////////////////////

matcher_ptr_t Parser::declare(std::string name) {
  //log_info_begin("DECLARE MATCHER<%s> ", name.c_str());
  auto it = _matchers_by_name.find(name);
  if (it != _matchers_by_name.end()) {
    //log_match_continue("pre-exists<%p>\n", (void*)it->second.get());
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
  //log_match_continue("new<%p>\n", (void*)rval.get());
  ///////////////////////////////////////////////
  return rval;
}

//////////////////////////////////////////////////////////////////////

void Parser::link() {
  std::set<matcher_ptr_t> unlinked;
  for (auto matcher : _matchers) {
    unlinked.insert(matcher);
  }
  bool keep_going       = true;
  int none_linked_count = 0;
  while (keep_going) {
    ////////////////////////////////////////////////
    std::set<matcher_ptr_t> pass_linked;
    ////////////////////////////////////////////////
    for (auto matcher : unlinked) {
      auto matcher_name = matcher->_name;
      bool OK           = true;
      if (matcher->_on_link) {
        OK = matcher->_on_link();
      }
      if (OK) {
        log_info("MATCHER<%s> LINKED...", matcher_name.c_str());
        pass_linked.insert(matcher);
      } else {
        log_info("MATCHER<%s> LINK FAILED, will reattempt..", matcher_name.c_str());
        matcher->_linkattempts++;
      }
    }
    ////////////////////////////////////////////////
    for (auto linked_item : pass_linked) {
      unlinked.erase(linked_item);
    }
    ////////////////////////////////////////////////
    if (pass_linked.size() == 0) {
      none_linked_count++;
    } else {
      none_linked_count = 0;
    }
    ////////////////////////////////////////////////
    keep_going = (none_linked_count >= 2);
  }
}

//////////////////////////////////////////////////////////////////////

match_attempt_ptr_t Parser::leafAttempt(matcher_ptr_t matcher) {

  match_attempt_ptr_t parent;
  if (not _match_stack.empty()) {
    parent = _match_stack.back();
  }
  auto rval      = std::make_shared<MatchAttempt>();
  rval->_parent  = parent;
  rval->_matcher = matcher;
  if (parent) {
    parent->_children.push_back(rval);
    // printf( "xxx : LEAFMATCH parent<%p:%s> rval<%p:%s>\n", (void*)parent.get(), parent->_matcher->_name.c_str(),
    // (void*)rval.get(), matcher->_name.c_str() );
  } else {
    // printf( "xxx : LEAFMATCH noparent rval<%p:%s>\n", (void*)rval.get(), matcher->_name.c_str() );
  }
  rval->_terminal = true;
  return rval;
}

//////////////////////////////////////////////////////////////////////

match_ptr_t Parser::createMatch(match_attempt_ptr_t ma) {
  _last_match = std::make_shared<Match>(ma);
  return _last_match;
}

//////////////////////////////////////////////////////////////////////

match_attempt_ptr_t Parser::pushAttempt(matcher_ptr_t matcher) {

  _track_depth++;
  OrkAssert(matcher);

  match_attempt_ptr_t parent;
  if (not _match_stack.empty()) {
    parent = _match_stack.back();
  }

  //////////////////////////////////////////////////////

  auto rval      = std::make_shared<MatchAttempt>();
  rval->_matcher = matcher;
  rval->_parent  = parent;
  if (parent) {
    OrkAssert(parent->_matcher);
    parent->_children.push_back(rval);
    // printf( "xxx : PUSHMATCH parent<%p:%s> rval<%p:%s>\n", (void*)parent.get(), parent->_matcher->_name.c_str(),
    // (void*)rval.get(), matcher->_name.c_str() );
  } else {
    // printf( "xxx : PUSHMATCH noparent rval<%p:%s>\n", (void*)rval.get(), matcher->_name.c_str() );
  }

  //////////////////////////////////////////////////////

  rval->_terminal = false;
  _match_stack.push_back(rval);

  return rval;
}

//////////////////////////////////////////////////////////////////////

void Parser::popAttempt(match_attempt_ptr_t attempt, matcher_ptr_t matcher, scannerlightview_constptr_t view) {

  //////////////////////////////////
  // parse track (for diagnostics)
  //////////////////////////////////
  if (attempt) {

      auto track      = std::make_shared<MatchAttemptTrackItem>();
      track->_matcher = matcher;
      track->_view    = view;

      if( _trackcontig ){
        if(view->_start!=-1 and view->_end!=-1){
          if( view->_start > _trackcontig->_view->_start ){
            _trackcontig    = track;
          }
        }
      }
      else{
        _trackcontig    = track;
      }

  } else {

    //if(_track_depth > _high_track_depth){
      //_high_track_depth = _track_depth;
      //auto track      = std::make_shared<MatchAttemptTrackItem>();
      //track->_matcher = matcher;
      //track->_view    = view;
      //_trackcontig    = track;
      //printf("_trackcontig<%p>\n", (void*)_trackcontig.get());
    //}

    if (_attempt_prev == nullptr and attempt == nullptr) {
      //printf("POP NOMATCH matcher<%s> view st<%zu> en<%zu>\n", matcher->_name.c_str(), view->_start, view->_end);
    }

  }
  _attempt_prev = attempt;

  _match_stack.pop_back();

  _track_depth--;
}

//////////////////////////////////////////////////////////////////////

match_attempt_ptr_t Parser::_tryMatch(MatchAttemptContextItem& mci) {

  auto matcher  = mci._matcher;
  auto inp_view = mci._view;

  uint64_t hash = matcher->hash(inp_view);
  auto it_hash  = _packrat_cache.find(hash);
  if (it_hash != _packrat_cache.end()) {
    auto cached_match_attempt = it_hash->second;
    if (cached_match_attempt->_view->_end == inp_view->_end) {
      _cache_hits = 0;
      return cached_match_attempt;
    }
  }

  _cache_misses++;

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
  // packrat cache
  //////////////////////////////////
  if (match_attempt) {
    _packrat_cache[hash] = match_attempt;
  }
  //////////////////////////////////
  return match_attempt;
}

//////////////////////////////////////////////////////////////

match_ptr_t Parser::match(
    matcher_ptr_t topmatcher, //
    scannerlightview_constptr_t topview,
    match_notif_t prelink_notif) {

  _cache_misses     = 0;
  _cache_hits       = 0;
  _track_depth      = 0;
  _high_track_depth = 0;

  if (topmatcher == nullptr) {
    logerrchannel()->log("Parser<%p> no top match function", this);
    OrkAssert(false);
  }
  _matchattemptctx._stack.clear();
  _matchattemptctx._topmatcher = topmatcher;
  _matchattemptctx._topview    = topview;
  MatchAttemptContextItem mci{topmatcher, topview};
  auto root_match_attempt = _tryMatch(mci);

  scannerlightview_constptr_t rm_view = root_match_attempt->_view;
  bool start_match                    = (rm_view->_start == topview->_start);
  bool end_match                      = (rm_view->_end == topview->_end);

  if ((not start_match) or (not end_match)) {

    scannerlightview_constptr_t errview;

    if (_trackcontig) {
      errview = _trackcontig->_view;
    }
    OrkAssert(errview);
    topview->dump("topview");

    root_match_attempt->dump1(0);

    auto errtok = topview->token(errview->_start);

    logerrchannel()->log("FULL MATCH FAILED");
    logerrchannel()->log("topview<%zu:%zu>", topview->_start, topview->_end);
    logerrchannel()->log("errview<%zu:%zu>", errview->_start, errview->_end);
    size_t errtok_lineno = 0;
    size_t errtok_colno  = 0;
    if (errtok) {
      errtok_lineno = errtok->iline;
      errtok_colno  = errtok->icol;
      logerrchannel()->log("errtok<%s> errtok_linenum<%zu> errtok_columnnum<%zu>", errtok->text.c_str(), errtok_lineno, errtok_colno);
    } else {
      logerrchannel()->log("NO END");
      exit(-1);
    }

    logerrchannel()->log("////////////////////// CURRENT POS (succeeded) ////////////////////// ");

    size_t st_line = std::max((errtok_lineno - 3), size_t(0));
    size_t en_line = st_line + 3 + 3 + 1;

    for (size_t cu_line = st_line; cu_line < en_line; cu_line++) {
      auto dbg_line = _scanner->_lines[cu_line];
      std::string str;
      if (cu_line == errtok_lineno) {
        str = deco::format(255, 255, 64, "line<%zu>: ", cu_line);
        str += deco::format(255, 255, 192, "%s", dbg_line.c_str());
      } else {
        str = deco::format(255, 64, 255, "line<%zu>: ", cu_line);
        str += deco::format(255, 192, 255, "%s", dbg_line.c_str());
      }
      printf("%s\n", str.c_str());
    }

    logerrchannel()->log("////////////////////// CURRENT POS (succeeded) ////////////////////// ");

    size_t st_tok = std::max((errview->_start - 9), size_t(0));
    size_t en_tok = st_tok + 9 + 9 + 1;

    for (size_t cu_tok = st_tok; cu_tok < en_tok; cu_tok++) {
      auto dbg_tok = _scanner->token(cu_tok);
      std::string str;
      if (cu_tok == errview->_start) {
        str = deco::format(255, 255, 64, "tok<%zu>: ", cu_tok);
        str += deco::format(255, 255, 192, "%s", dbg_tok->text.c_str());
      } else {
        str = deco::format(255, 64, 255, "tok<%zu>: ", cu_tok);
        str += deco::format(255, 192, 255, "%s", dbg_tok->text.c_str());
      }
      printf("%s\n", str.c_str());
    }

    logerrchannel()->log("///////////////////////////////////////////////////////////////////// ");


    OrkAssert(false);
  }

  if (root_match_attempt) {
    auto root_match = MatchAttempt::genmatch(root_match_attempt);
    OrkAssert(root_match);
    _visitComposeMatch(root_match);
    if (prelink_notif) {
      prelink_notif(root_match);
    }
    _visitLinkMatch(root_match);

    log_info("CACHE_HITS<%zu> CACHE_MISSES<%zu>\n", _cache_hits, _cache_misses);

    return root_match;
  }

  return nullptr;
}

//////////////////////////////////////////////////////////////////////

void Parser::_visitComposeMatch(match_ptr_t m) {

  if (true) {
    auto indentstr = std::string(_visit_depth * 2, ' ');
    std::string suffix;
    if (m->_matcher->_pre_notif) {
      suffix += " PRE";
    }
    if (m->_matcher->_post_notif) {
      suffix += " POST";
    }
    if (m->_matcher->_link_notif) {
      suffix += " LINK";
    }

    if (0)
      printf(
          "xxx : %s match<%p:%s:%s> numc<%zu> view<%zu:%zu> %s\n", //
          indentstr.c_str(),                                       //
          (void*)m.get(),                                          //
          m->_matcher->_name.c_str(),                              //
          m->_matcher->_info.c_str(),                              //
          m->_children.size(),                                     //
          m->_view->_start,
          m->_view->_end, //
          suffix.c_str());
  }

  _visit_depth++;
  if (m->_matcher->_pre_notif) {
    m->_matcher->_pre_notif(m);
  }
  for (auto& child_match : m->_children) {
    _visitComposeMatch(child_match);
  }
  if (m->_matcher->_post_notif) {
    m->_matcher->_post_notif(m);
  }
  _visit_depth--;
}

//////////////////////////////////////////////////////////////////////

void Parser::_visitLinkMatch(match_ptr_t m) {

  auto indentstr = std::string(_visit_depth * 2, ' ');
  std::string suffix;

  _visit_depth++;
  for (auto& child_match : m->_children) {
    _visitLinkMatch(child_match);
  }
  if (m->_matcher->_link_notif) {
    m->_matcher->_link_notif(m);
  }
  _visit_depth--;
}

//////////////////////////////////////////////////////////////////////

static constexpr size_t KVIEWLEN = 16;

void Parser::_log_valist(scannerlightview_constptr_t view, const char* pMsgFormat, va_list args) const {
  char buf[256];
  vsnprintf_s(buf, sizeof(buf), pMsgFormat, args);
  size_t indent  = _matchattemptctx._stack.size();
  std::string indentstr;
  if( view ){
    auto viewstr = FormatString("view<%zd:%zd>", view->_start, view->_end);
    size_t vlen = std::max(viewstr.length(), KVIEWLEN);
    indent = KVIEWLEN+indent-vlen;
    indentstr = viewstr+std::string(indent, ' ');
  }
  else{
    indentstr = std::string(indent, ' ');
  }
  printf("[PARSER] %s%s\n", indentstr.c_str(), buf);
}
void Parser::_log_valist_begin(scannerlightview_constptr_t view, const char* pMsgFormat, va_list args) const {
  char buf[256];
  vsnprintf_s(buf, sizeof(buf), pMsgFormat, args);
  size_t indent  = _matchattemptctx._stack.size();
  std::string indentstr;
  if( view ){
    auto viewstr = FormatString("view<%zd:%zd>", view->_start, view->_end);
    size_t vlen = std::max(viewstr.length(), KVIEWLEN);
    indent = KVIEWLEN+indent-vlen;
    indentstr = viewstr+std::string(indent, ' ');
  }
  else{
    indentstr = std::string(indent, ' ');
  }
  printf("[PARSER] %s%s", indentstr.c_str(), buf);
}
void Parser::_log_valist_continue(const char* pMsgFormat, va_list args) const {
  char buf[256];
  vsnprintf_s(buf, sizeof(buf), pMsgFormat, args);
  printf("%s", buf);
}

//////////////////////////////////////////////////////////////////////

void Parser::log_match(scannerlightview_constptr_t view, const char* pMsgFormat, ...) const {
  if (_DEBUG_MATCH) {
    va_list args;
    va_start(args, pMsgFormat);
    _log_valist(view, pMsgFormat, args);
    va_end(args);
  }
}
void Parser::log_match_begin(scannerlightview_constptr_t view, const char* pMsgFormat, ...) const {
  if (_DEBUG_MATCH) {
    va_list args;
    va_start(args, pMsgFormat);
    _log_valist_begin(view, pMsgFormat, args);
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
    _log_valist(nullptr, pMsgFormat, args);
    va_end(args);
  }
}
void Parser::log_info_begin(const char* pMsgFormat, ...) const {
  if (_DEBUG_INFO) {
    va_list args;
    va_start(args, pMsgFormat);
    _log_valist_begin(nullptr, pMsgFormat, args);
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
