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
static logchannel_ptr_t logchan_parser_topo = logger()->createChannel("PARSER_TOPO", fvec3(0.5, 0.7, 0.5), true);
static std::atomic<int> g_matcher_id(0);
//////////////////////////////////////////////////////////////////////

match_attempt_ptr_t filtered_match(matcher_ptr_t matcher, match_attempt_ptr_t the_match) {
  if (the_match) {
    auto filter = matcher->_match_filter;
    if (filter) {
      bool good = filter(the_match);
      if (not good) {
        the_match = nullptr;
      }
    }
  }
  return the_match;
}

//////////////////////////////////////////////////////////////////////

void Parser::_proxy(matcher_ptr_t par_matcher, matcher_ptr_t sub_matcher){
  /////////////////////////////////
  par_matcher->_genmatch_fn = [=](match_attempt_ptr_t attempt) -> match_ptr_t {
    auto the_proxy_attempt = attempt->asShared<ProxyAttempt>();
    if (the_proxy_attempt->_selected) {
      auto the_match            = std::make_shared<Match>(attempt);
      auto the_proxy_inst       = the_match->makeShared<Proxy>();
      the_proxy_inst->_selected = MatchAttempt::genmatch(the_proxy_attempt->_selected);
      the_match->_children.push_back(the_proxy_inst->_selected);
      return the_match;
    }
    return nullptr;
  };
  /////////////////////////////////
  par_matcher->_attempt_match_fn = [=](matcher_ptr_t par_matcher,                                //
                                   scannerlightview_constptr_t slv) -> match_attempt_ptr_t { //
    auto tok0 = slv->token(0);

    auto match_attempt = pushMatch(par_matcher);
    auto the_proxy     = match_attempt->makeShared<ProxyAttempt>();

    MatchAttemptContextItem mci{sub_matcher, slv};
    auto sub_match = _tryMatch(mci);
    if (sub_match) {
      auto match_str = deco::string("MATCH", 0, 255, 0);
      match_attempt->_view = sub_match->_view;
      match_attempt->_view->validate();
      the_proxy->_selected = sub_match;
    } else {
      match_attempt = nullptr;
    }
    popMatch();
    return match_attempt;
  };
  //par_matcher->_pre_notif        = sub_matcher->_pre_notif;
  //par_matcher->_post_notif        = sub_matcher->_post_notif;
}

//////////////////////////////////////////////////////////////////////

matcher_ptr_t Parser::optional(matcher_ptr_t sub_matcher, std::string name) {
  if (name == "") {
    name = FormatString("optional-%d", g_matcher_id++);
  }
  ///////////////////////////////////////////////////////
  auto matcher   = declare(name);
  matcher->_info = FormatString("OPT");
  ///////////////////////////////////////////////////////
  matcher->_genmatch_fn = [=](match_attempt_ptr_t attempt) -> match_ptr_t {
    auto the_opt_attempt = attempt->asShared<OptionalAttempt>();
    auto the_match       = std::make_shared<Match>(attempt);
    auto the_opt_inst    = the_match->makeShared<Optional>();
    if (the_opt_attempt->_subitem) {
      the_opt_inst->_subitem = MatchAttempt::genmatch(the_opt_attempt->_subitem);
      the_match->_children.push_back(the_opt_inst->_subitem);
    }
    return the_match;
  };
  ///////////////////////////////////////////////////////
  matcher->_attempt_match_fn = [=](matcher_ptr_t par_matcher,                                //
                                   scannerlightview_constptr_t slv) -> match_attempt_ptr_t { //
    auto match_attempt = pushMatch(par_matcher);
    auto the_opt       = match_attempt->makeShared<OptionalAttempt>();
    /////////////////////////////////////
    // if sub_matcher matches, return the match
    ////////////////////////////////////////////////////
    if (not slv->empty()) {
      MatchAttemptContextItem mci{sub_matcher, slv};
      auto sub_match = _tryMatch(mci);
      if (sub_match) {
        auto match_str = deco::string("MATCH", 0, 255, 0);
        log_match("OPT<%s> %s sub<%s>", name.c_str(), match_str.c_str(), sub_match->_matcher->_name.c_str());
        match_attempt->_view = sub_match->_view;
        match_attempt->_view->validate();
        the_opt->_subitem = sub_match;
        popMatch();
        return match_attempt;
      }
    }
    auto empty_str = deco::string("EMPTY", 255, 255, 0);
    log_match("OPT<%s> %s", name.c_str(), empty_str.c_str());
    ////////////////////////////////////////////////////
    //  otherwise return "empty" but valid match (start=end=-1)
    ////////////////////////////////////////////////////
    match_attempt->_view = std::make_shared<ScannerLightView>(*slv);
    match_attempt->_view->clear();
    auto rval = filtered_match(par_matcher, match_attempt);
    popMatch();
    return rval;
  };
  ///////////////////////////////////////////////////////
  return matcher;
}

//////////////////////////////////////////////////////////////////////

void Parser::_sequence(matcher_ptr_t matcher, std::vector<matcher_ptr_t> sub_matchers) {
  ///////////////////////////////////////////////////////
  matcher->_genmatch_fn = [=](match_attempt_ptr_t attempt) -> match_ptr_t {
    auto the_seq_attempt = attempt->asShared<SequenceAttempt>();
    auto the_match       = std::make_shared<Match>(attempt);
    auto the_seq_inst    = the_match->makeShared<Sequence>();
    for (auto item : the_seq_attempt->_items) {
      auto sub_match = MatchAttempt::genmatch(item);
      the_seq_inst->_items.push_back(sub_match);
      the_match->_children.push_back(sub_match);
    }
    return the_match;
  };
  ///////////////////////////////////////////////////////
  matcher->_attempt_match_fn = [=](matcher_ptr_t par_matcher, scannerlightview_constptr_t slv) -> match_attempt_ptr_t {
    auto match_attempt = pushMatch(par_matcher);
    log_match("SEQ<%s>: beg_match len<%zu>", matcher->_name.c_str(), sub_matchers.size());
    auto slv_iter        = std::make_shared<ScannerLightView>(*slv);
    auto slv_match       = std::make_shared<ScannerLightView>(*slv);
    auto the_sequence    = match_attempt->makeShared<SequenceAttempt>();
    match_attempt->_view = slv_match;
    size_t iter          = 0;
    size_t num_iter      = sub_matchers.size();
    for (auto sub_matcher : sub_matchers) {
      if (0)
        log_match_begin(
            "SEQSS<%s> : match_item<%s> iter<%zu/%zu> itrst<%d> itren<%d> slvst<%d> slvend<%d> ",
            matcher->_name.c_str(),
            sub_matcher->_name.c_str(),
            iter,
            num_iter,
            slv_iter->_start,
            slv_iter->_end,
            slv->_start,
            slv->_end);
      if (slv_iter->_start > slv_iter->_end) {
        break;
      }
      auto tok0 = slv_iter->token(0);
      MatchAttemptContextItem mci{sub_matcher, slv_iter};
      auto match_item = _tryMatch(mci);
      log_match_begin(
          "SEQ<%s> : match_item<%s> tok0<%s> iter<%zu/%zu> st<%d> end<%d> ",
          matcher->_name.c_str(),
          sub_matcher->_name.c_str(),
          tok0->text.c_str(),
          iter,
          num_iter,
          slv_iter->_start,
          slv_iter->_end);
      if (match_item) {
        size_t item_index = the_sequence->_items.size();
        if (match_item->_view->empty()) {
          auto empty_str = deco::string("MATCH: EMPTY", 0, 255, 255);
          log_match_continue("%s\n", empty_str.c_str());
        } else {
          if (0)
            slv_iter->dump("slv_iter_pre");
          auto match_str = deco::string("MATCH", 0, 255, 0);
          log_match_continue("%s\n", match_str.c_str());
          slv_match->_end  = match_item->_view->_end;
          slv_iter->_start = match_item->_view->_end + 1;
          if (0)
            slv_iter->dump("slv_iter_post");
          // slv_iter->validate();
          // slv_match->validate();
        }
        the_sequence->_items.push_back(match_item);
        OrkAssert(match_item->_view);
      } else {
        auto match_str = deco::string("NO-MATCH", 255, 0, 0);
        log_match_continue("%s\n", match_str.c_str());
        match_attempt = nullptr;
        break;
      }
      iter++;
    }
    if (match_attempt) {
      /////////////////////////////////////////////////
      // prune
      /////////////////////////////////////////////////
      int num_matches = the_sequence->_items.size();
      int num_emptys  = 0;
      for (auto match : the_sequence->_items) {
        if (match->_view->empty()) {
          num_emptys++;
        }
      }
      if (num_emptys == num_matches) {
        match_attempt->_view->clear();
      } else {
        auto slv_out = std::make_shared<ScannerLightView>(*slv);
        for (auto match : the_sequence->_items) {
          if (not match->_view->empty()) {
            slv_out->_end = match->_view->_end;
          }
        }
        match_attempt->_view = slv_out;
      }
      /////////////////////////////////////////////////
      if (0)
        log_match(
            "SEQ<%s> end match_attempt<%p> st<%zu> en<%zu> count<%zu>", //
            matcher->_name.c_str(),                                     //
            (void*)match_attempt.get(),                                 //
            match_attempt->_view->_start,                               //
            match_attempt->_view->_end,                                 //
            the_sequence->_items.size());
    } else {
      if (0)
        log_match("SEQ<%s> end NO_MATCH", matcher->_name.c_str());
    }
    auto rval = filtered_match(par_matcher, match_attempt);
    popMatch();
    return rval;
  };
  ///////////////////////////////////////////////////////
}

//////////////////////////////////////////////////////////////////////

matcher_ptr_t Parser::sequence(std::vector<matcher_ptr_t> sub_matchers, std::string name) {
  if (name == "") {
    name = FormatString("sequence-%d", g_matcher_id++);
  }
  ///////////////////////////////////////////////////////
  auto matcher   = declare(name);
  matcher->_info = FormatString("SEQ");
  ///////////////////////////////////////////////////////
  _sequence(matcher, sub_matchers);
  ///////////////////////////////////////////////////////
  return matcher;
}

//////////////////////////////////////////////////////////////////////

matcher_ptr_t Parser::sequence(std::string name, std::vector<matcher_ptr_t> matchers) {
  return sequence(matchers, name);
}

//////////////////////////////////////////////////////////////////////

matcher_ptr_t Parser::group(std::vector<matcher_ptr_t> matchers, std::string name) {
  if (name == "") {
    name = FormatString("group-%d", g_matcher_id++);
  }
  ///////////////////////////////////////////////////////
  auto matcher   = declare(name);
  matcher->_info = FormatString("GRP");
  ///////////////////////////////////////////////////////
  matcher->_genmatch_fn = [=](match_attempt_ptr_t attempt) -> match_ptr_t {
    auto the_grp_attempt = attempt->asShared<GroupAttempt>();
    auto the_match       = std::make_shared<Match>(attempt);
    auto the_grp_inst    = the_match->makeShared<Group>();
    for (auto item : the_grp_attempt->_items) {
      auto sub_match = MatchAttempt::genmatch(item);
      the_grp_inst->_items.push_back(sub_match);
      the_match->_children.push_back(sub_match);
    }
    return the_match;
  };
  ///////////////////////////////////////////////////////
  matcher->_attempt_match_fn = [=](matcher_ptr_t par_matcher, scannerlightview_constptr_t slv_inp) -> match_attempt_ptr_t {
    auto match_attempt = pushMatch(par_matcher);
    auto slv_iter      = std::make_shared<ScannerLightView>(*slv_inp);
    auto the_group     = match_attempt->makeShared<GroupAttempt>();
    for (auto sub_matcher : matchers) {
      MatchAttemptContextItem mci{sub_matcher, slv_iter};
      auto sub_match = _tryMatch(mci);
      if (sub_match) {
        the_group->_items.push_back(sub_match);
        slv_iter->_start = sub_match->_view->_end + 1;
      } else {
        popMatch();
        return nullptr;
      }
    }
    OrkAssert(the_group->_items.size());
    auto slv_out    = std::make_shared<ScannerLightView>(*slv_inp);
    slv_out->_start = the_group->_items.front()->_view->_start;
    slv_out->_end   = the_group->_items.back()->_view->_end;
    slv_out->validate();
    match_attempt->_view = slv_out;
    auto rval            = filtered_match(par_matcher, match_attempt);
    popMatch();
    return rval;
  };
  ///////////////////////////////////////////////////////
  return matcher;
}

//////////////////////////////////////////////////////////////////////

matcher_ptr_t Parser::oneOf(std::vector<matcher_ptr_t> matchers, std::string name) {
  if (name == "") {
    name = FormatString("oneof-%d", g_matcher_id++);
  }
  ///////////////////////////////////////////////////////
  auto matcher   = declare(name);
  matcher->_info = FormatString("1OF");
  ///////////////////////////////////////////////////////
  matcher->_genmatch_fn = [=](match_attempt_ptr_t attempt) -> match_ptr_t {
    auto the_oof_attempt    = attempt->asShared<OneOfAttempt>();
    auto the_match          = std::make_shared<Match>(attempt);
    auto the_oof_inst       = the_match->makeShared<OneOf>();
    the_oof_inst->_selected = MatchAttempt::genmatch(the_oof_attempt->_selected);
    the_match->_children.push_back(the_oof_inst->_selected);
    return the_match;
  };
  ///////////////////////////////////////////////////////
  matcher->_attempt_match_fn = [=](matcher_ptr_t par_matcher, scannerlightview_constptr_t slv) -> match_attempt_ptr_t {
    auto match_attempt = pushMatch(par_matcher);
    // log_match( "oneOf<%s>: begin num_subs<%zu>\n", name.c_str(), matchers.size() );
    for (auto sub_matcher : matchers) {
      MatchAttemptContextItem mci{sub_matcher, slv};
      auto sub_match = _tryMatch(mci);
      if (sub_match) {
        auto match_str = deco::string("MATCH", 0, 255, 0);
        log_match("1OF<%s>: %s sub_matcher<%s>", name.c_str(), match_str.c_str(), sub_matcher->_name.c_str());

        auto the_oo          = match_attempt->makeShared<OneOfAttempt>();
        the_oo->_selected    = sub_match;
        match_attempt->_view = sub_match->_view;
        popMatch();
        return filtered_match(par_matcher, match_attempt);
      }
    }
    auto match_str = deco::string("NO-MATCH", 255, 0, 0);
    log_match("1OF<%s>: %s", name.c_str(), match_str.c_str());
    popMatch();
    return nullptr;
  };
  ///////////////////////////////////////////////////////
  return matcher;
}

//////////////////////////////////////////////////////////////////////

matcher_ptr_t Parser::oneOf(std::string name, std::vector<matcher_ptr_t> matchers) {
  return oneOf(matchers, name);
}
//////////////////////////////////////////////////////////////////////

matcher_ptr_t Parser::nOrMore(matcher_ptr_t sub_matcher, size_t minMatches, std::string name, bool mustConsumeAll) {
  if (name == "") {
    name = FormatString("(%d)OrMore-%d", minMatches, g_matcher_id++);
  }
  //////////////////////////////////////////////////////
  auto matcher   = declare(name);
  matcher->_info = FormatString("NOM%zu", minMatches);
  //////////////////////////////////////////////////////
  matcher->_genmatch_fn = [=](match_attempt_ptr_t attempt) -> match_ptr_t {
    auto the_nom_attempt          = attempt->asShared<NOrMoreAttempt>();
    auto the_match                = std::make_shared<Match>(attempt);
    auto the_nom_inst             = the_match->makeShared<NOrMore>();
    the_nom_inst->_minmatches     = the_nom_attempt->_minmatches;
    the_nom_inst->_mustConsumeAll = the_nom_attempt->_mustConsumeAll;
    for (auto item : the_nom_attempt->_items) {
      auto sub_match = MatchAttempt::genmatch(item);
      the_nom_inst->_items.push_back(sub_match);
      the_match->_children.push_back(sub_match);
    }
    return the_match;
  };
  //////////////////////////////////////////////////////
  matcher->_attempt_match_fn = [=](matcher_ptr_t par_matcher, scannerlightview_constptr_t input_slv) -> match_attempt_ptr_t {
    auto match_attempt       = pushMatch(par_matcher);
    auto the_nom             = match_attempt->makeShared<NOrMoreAttempt>();
    the_nom->_mustConsumeAll = mustConsumeAll;
    the_nom->_minmatches     = minMatches;
    bool keep_going          = true;
    auto slv_iter            = std::make_shared<ScannerLightView>(*input_slv);
    int item_index           = 0;
    log_match("NOM%zu<%s>: beg_match sub_matcher<%s>", minMatches, name.c_str(), sub_matcher->_name.c_str());
    ////////////////////////////////////////////////////////////////
    while (keep_going) {
      MatchAttemptContextItem mci{sub_matcher, slv_iter};
      auto sub_match = _tryMatch(mci);
      keep_going     = false;
      log_match_begin(
          "NOM%d<%s> try_match iter<%d> ", //
          int(minMatches),
          name.c_str(),
          item_index);
      if (sub_match) {
        the_nom->_items.push_back(sub_match);
        item_index++;
        slv_iter->_start = sub_match->_view->_end + 1;
        keep_going       = slv_iter->_start <= slv_iter->_end;
        if (sub_match->_view->empty()) {
          keep_going     = false;
          auto empty_str = deco::string("EMPTY", 0, 255, 255);
          log_match_continue("%s\n", empty_str.c_str());
        } else {
          auto match_str = deco::string("MATCH", 0, 255, 0);
          log_match_continue("%s start<%zu> end<%zu>\n", match_str.c_str(), sub_match->_view->_start, sub_match->_view->_end);
        }
      } else {
        auto nomatch_str = deco::string("NO-MATCH", 255, 0, 0);
        log_match_continue("%s\n", nomatch_str.c_str());
      }
    }
    ////////////////////////////////////////////////////////////////
    if (the_nom->_items.size() >= minMatches) {
      auto slv_out = std::make_shared<ScannerLightView>(*input_slv);
      if (minMatches == 0 and the_nom->_items.size() == 0) {
        slv_out->clear();
      } else {
        slv_out->_start = the_nom->_items.front()->_view->_start;
        slv_out->_end   = the_nom->_items.back()->_view->_end;
        slv_out->validate();
      }
      match_attempt->_view = slv_out;
      if ((slv_out->_end < input_slv->_end) and the_nom->_mustConsumeAll) {
        auto no_match_str = deco::string("NO-MATCH (not all tokens consumed)", 255, 0, 0);
        log_match("NOM%zu<%s>: %s count<%zu>", minMatches, name.c_str(), no_match_str.c_str(), the_nom->_items.size());
        popMatch();
        return nullptr;
      } else {
        auto match_str = deco::string("MATCH", 0, 255, 0);
        log_match(
            "NOM%zu<%s>: %s count<%zu> st<%zu> en<%zu> slen<%d>",
            minMatches,
            name.c_str(),
            match_str.c_str(),
            the_nom->_items.size(),
            slv_out->_start,
            slv_out->_end,
            input_slv->_end);
      }
      auto rval = filtered_match(par_matcher, match_attempt);
      popMatch();
      return rval;
    } else if (minMatches == 0) {
      auto slv_out = std::make_shared<ScannerLightView>(*input_slv);
      slv_out->clear();
      match_attempt->_view = slv_out;
      auto no_match_str    = deco::string("NO-MATCH", 255, 0, 0);
      log_match("NOM%zu<%s>: %s count<0>", minMatches, name.c_str(), no_match_str.c_str());
      auto rval = filtered_match(par_matcher, match_attempt);
      popMatch();
      return rval;
    }
    OrkAssert(false); // should never get here?
    // log_match( "NOM%zu<%s>: end_match (NOMATCH)", minMatches, name.c_str() );
    popMatch();
    return nullptr;
  };
  return matcher;
}

matcher_ptr_t Parser::oneOrMore(matcher_ptr_t matcher, std::string name) {
  return nOrMore(matcher, 1, name);
}
matcher_ptr_t Parser::zeroOrMore(matcher_ptr_t matcher, std::string name, bool mustConsumeAll) {
  return nOrMore(matcher, 0, name, mustConsumeAll);
}

//////////////////////////////////////////////////////////////

matcher_ptr_t Parser::matcherForTokenClassID(uint64_t tokclass, std::string name) {
  auto matcher   = declare(name);
  matcher->_info = FormatString("TOKCLASS<%zx>", tokclass);
  ///////////////////////////////////////////////////////
  matcher->_genmatch_fn = [=](match_attempt_ptr_t attempt) -> match_ptr_t {
    auto class_match_attempt = attempt->asShared<ClassMatchAttempt>();
    auto the_match           = std::make_shared<Match>(attempt);
    auto cm_inst             = the_match->makeShared<ClassMatch>();
    cm_inst->_tokclass       = class_match_attempt->_tokclass;
    cm_inst->_token          = class_match_attempt->_token;
    return the_match;
  };
  ///////////////////////////////////////////////////////
  matcher->_attempt_match_fn = [tokclass, this](matcher_ptr_t par_matcher, scannerlightview_constptr_t slv) -> match_attempt_ptr_t {
    auto tok0         = slv->token(0);
    auto slv_tokclass = tok0->_class;
    log_match(
        "MATCHER.CLASSID<%zu> matcher_name<%s> tok<%s> tokclass<%zu>", //
        tokclass,
        par_matcher->_name.c_str(),
        tok0->text.c_str(),
        slv_tokclass);
    if (slv_tokclass == tokclass) {
      auto match_attempt        = leafMatch(par_matcher);
      auto the_classmatch       = match_attempt->makeShared<ClassMatchAttempt>();
      the_classmatch->_tokclass = tokclass;
      the_classmatch->_token    = tok0;
      auto slv_out              = std::make_shared<ScannerLightView>(*slv);
      slv_out->_end             = slv_out->_start;
      match_attempt->_view      = slv_out;
      slv_out->validate();
      auto rval = filtered_match(par_matcher, match_attempt);
      return rval;
    }
    return nullptr;
  };
  ///////////////////////////////////////////////////////
  return matcher;
}

//////////////////////////////////////////////////////////////

matcher_ptr_t Parser::matcherForWord(std::string word, std::string name) {
  if (name == "") {
    name = word;
  }
  ///////////////////////////////////////////////////////
  auto matcher   = declare(name);
  matcher->_info = FormatString("WORD<%s>", word.c_str());
  ///////////////////////////////////////////////////////
  matcher->_genmatch_fn = [=](match_attempt_ptr_t attempt) -> match_ptr_t {
    auto word_match_attempt = attempt->asShared<WordMatchAttempt>();
    auto the_match          = std::make_shared<Match>(attempt);
    auto wm_inst            = the_match->makeShared<WordMatch>();
    wm_inst->_token         = word_match_attempt->_token;
    return the_match;
  };
  ///////////////////////////////////////////////////////
  matcher->_attempt_match_fn = [=](matcher_ptr_t par_matcher, scannerlightview_constptr_t inp_view) -> match_attempt_ptr_t {
    auto tok0 = inp_view->token(0);
    if (tok0->text == word) {
      auto slv              = std::make_shared<ScannerLightView>(*inp_view);
      slv->_end             = slv->_start;
      auto match_attempt    = leafMatch(par_matcher);
      auto the_wordmatch    = match_attempt->makeShared<WordMatchAttempt>();
      the_wordmatch->_token = tok0;
      match_attempt->_view  = slv;
      slv->validate();
      return filtered_match(par_matcher, match_attempt);
    } else {
      return nullptr;
    }
  };
  ///////////////////////////////////////////////////////
  return matcher;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork
