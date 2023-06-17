////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <regex>
#include <stdlib.h>
#include <ork/pch.h>
#include <ork/file/file.h>
#include <ork/util/parser.h>
#include <ork/kernel/string/deco.inl>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork {
/////////////////////////////////////////////////////////////////////////////////////////////////

void Match::dump(int indent) const {

    auto indentstr = std::string(indent, ' ');

    if( _view->empty() ){
        printf( "%s DUMP Match<%p> matcher<%p:%s> view (empty)\n", indentstr.c_str(), this, (void*) _matcher.get(), _matcher->_name.c_str() );
    }
    else{
        printf( "%s DUMP Match<%p> matcher<%p:%s> view [%zu..%zu]\n", indentstr.c_str(), this, (void*) _matcher.get(), _matcher->_name.c_str(), _view->_start, _view->_end );
    }
    
    if( auto as_seq = _impl.tryAs<sequence_ptr_t>() ){
        auto seq = as_seq.value();
        printf( "%s   SEQ<%p>\n", indentstr.c_str(), (void*) seq.get() );
        for( auto i : seq->_items ){
            i->dump(indent+3);
        }
    }
    else if( auto as_zom = _impl.tryAs<zero_or_more_ptr_t>() ){
        auto zom = as_zom.value();
        printf( "%s   ZOM<%p>\n", indentstr.c_str(), (void*) zom.get() );
        for( auto i : zom->_items ){
            i->dump(indent+3);
        }
    }
    else if( auto as_oom = _impl.tryAs<one_or_more_ptr_t>() ){
        auto oom = as_oom.value();
        printf( "%s   OOM<%p>\n", indentstr.c_str(), (void*) oom.get() );
        for( auto i : oom->_items ){
            i->dump(indent+3);
        }
    }
    else if( auto as_grp = _impl.tryAs<group_ptr_t>() ){
        auto grp = as_grp.value();
        printf( "%s   GRP<%p>\n", indentstr.c_str(), (void*) grp.get() );
        for( auto i : grp->_items ){
            i->dump(indent+3);
        }
    }
}

Matcher::Matcher(matcher_fn_t match_fn)
    : _match_fn(match_fn) {
}

//////////////////////////////////////////////////////////////////////

matcher_ptr_t Parser::optional(matcher_ptr_t sub_matcher,std::string name) {
  auto match_fn = [=]( matcher_ptr_t par_matcher, //
                       scannerlightview_constptr_t slv) -> match_ptr_t { //

    auto the_match = std::make_shared<Match>();
    the_match->_matcher = par_matcher;
    ////////////////////////////////////////////////////
    // if sub_matcher matches, return the match
    ////////////////////////////////////////////////////
    auto sub_match = _match(sub_matcher,slv);
    if (sub_match) {
      //printf( "OPTIONAL sub_match<%p>\n", (void*) sub_match.get() );
      the_match->_view = sub_match->_view;
      return the_match;
    }
    //printf( "OPTIONAL NO sub_match\n" );
    ////////////////////////////////////////////////////
    //  otherwise return "empty" but valid match (start=end=-1)
    ////////////////////////////////////////////////////
    the_match->_view    = std::make_shared<ScannerLightView>(*slv);
    the_match->_view->clear();
    return the_match;
  };
  if(name==""){
     name = "optional";
  }
  return createMatcher(match_fn,name);}

//////////////////////////////////////////////////////////////////////

matcher_ptr_t Parser::sequence(std::vector<matcher_ptr_t> matchers,std::string name) {
  auto match_fn = [=](matcher_ptr_t par_matcher, scannerlightview_constptr_t slv) -> match_ptr_t {
    match_ptr_t the_match = std::make_shared<Match>();
    the_match->_matcher = par_matcher;
    //printf( "beg_match sequence<%s> len<%zu>\n", name.c_str(), matchers.size() );
    auto slv_temp  = std::make_shared<ScannerLightView>(*slv);
    auto slv_match = std::make_shared<ScannerLightView>(*slv);
    auto the_sequence = the_match->_impl.makeShared<Sequence>();
    the_match->_view = slv_match;
    for (auto sub_matcher : matchers) {
      auto match_item = _match(sub_matcher,slv_temp);
        //printf( "SLV_match sequence<%s> match_item<%p>\n", name.c_str(), (void*) match_item.get() );
      if (match_item) {
        if( not match_item->_view->empty() ){
            slv_match->_end  = match_item->_view->_end;
            slv_temp->_start = match_item->_view->_end + 1;
        }
        the_sequence->_items.push_back(match_item);
        // slv_temp->dump("slv_temp");
        OrkAssert(match_item->_view);
        //printf( "end_match sequence<%s> the_match<%p> st<%zu> en<%zu>\n", name.c_str(), (void*) the_match.get(), the_match->_view->_start, the_match->_view->_end );
      } else {
        the_match = nullptr;
        //printf( "end_match sequence<%s> NO_MATCH\n", name.c_str() );
        break;
      }
    }
    return the_match;
  };
  if(name==""){
     name = "sequence";
  }
  return createMatcher(match_fn,name);
}

//////////////////////////////////////////////////////////////////////

matcher_ptr_t Parser::sequence(std::string name, std::vector<matcher_ptr_t> matchers) {
    return sequence(matchers, name);
}

//////////////////////////////////////////////////////////////////////

matcher_ptr_t Parser::group(std::vector<matcher_ptr_t> matchers,std::string name) {
  auto match_fn = [=](matcher_ptr_t par_matcher, scannerlightview_constptr_t slv_inp) -> match_ptr_t {
    match_ptr_t the_match = std::make_shared<Match>();
    the_match->_matcher = par_matcher;
    auto slv_iter = std::make_shared<ScannerLightView>(*slv_inp);
    auto the_group = the_match->_impl.makeShared<Group>();
    for (auto sub_matcher : matchers) {
      auto sub_match = _match(sub_matcher,slv_iter);
      if (sub_match) {
        the_group->_items.push_back(sub_match);
        slv_iter->_start = sub_match->_view->_end + 1;
      } else {
        return nullptr;
      }
    }
    OrkAssert(the_group->_items.size());
    auto slv_out    = std::make_shared<ScannerLightView>(*slv_inp);
    slv_out->_start = the_group->_items.front()->_view->_start;
    slv_out->_end   = the_group->_items.back()->_view->_end;
    the_match->_view = slv_out;
    return the_match;
  };
  if(name==""){
     name = "group";
  }
  return createMatcher(match_fn,name);
}

//////////////////////////////////////////////////////////////////////

matcher_ptr_t Parser::oneOf(std::vector<matcher_ptr_t> matchers,std::string name) {
  auto match_fn = [=](matcher_ptr_t par_matcher, scannerlightview_constptr_t slv) -> match_ptr_t {
    for (auto sub_matcher : matchers) {
      auto sub_match = _match(sub_matcher,slv);
      if (sub_match) {
        return sub_match;
      }
    }
    return nullptr;
  };
  if(name==""){
     name = "oneOf";
  }
  return createMatcher(match_fn,name);
}

//////////////////////////////////////////////////////////////////////

matcher_ptr_t Parser::oneOrMore(matcher_ptr_t sub_matcher,std::string name) {
  auto match_fn = [=](matcher_ptr_t par_matcher, scannerlightview_constptr_t input_slv) -> match_ptr_t {
    match_ptr_t the_match = std::make_shared<Match>();
    the_match->_matcher = par_matcher;
    auto the_oom = the_match->_impl.makeShared<OneOrMore>();
    bool keep_going = true;
    auto slv_iter    = std::make_shared<ScannerLightView>(*input_slv);
    while (keep_going) {
      auto sub_match = _match(sub_matcher,slv_iter);
      keep_going = false;
      if (sub_match) {
        the_oom->_items.push_back(sub_match);
        slv_iter->_start = sub_match->_view->_end + 1;
        keep_going       = slv_iter->_start <= slv_iter->_end;
      }
    }
    if (the_oom->_items.size()) {
      auto slv_out    = std::make_shared<ScannerLightView>(*input_slv);
      slv_out->_start = the_oom->_items.front()->_view->_start;
      slv_out->_end   = the_oom->_items.back()->_view->_end;
      the_match->_view = slv_out;
      return the_match;
    }
    return nullptr;
  };
  if(name==""){
     name = "oneOrMore";
  }
  return createMatcher(match_fn,name);
}

//////////////////////////////////////////////////////////////////////

matcher_ptr_t Parser::zeroOrMore(matcher_ptr_t sub_matcher,std::string name) {
  auto match_fn = [=](matcher_ptr_t par_matcher, scannerlightview_constptr_t input_slv) -> match_ptr_t {
    match_ptr_t the_match = std::make_shared<Match>();
    the_match->_matcher = par_matcher;
    auto the_zom = the_match->_impl.makeShared<ZeroOrMore>();
    bool keep_going = true;
    auto slv_iter    = std::make_shared<ScannerLightView>(*input_slv);
    //printf( "ZOM input_slv_st<%zu> input_slv_end<%zu>>\n", input_slv->_start, input_slv->_end );
    while (keep_going) {
      auto sub_match = _match(sub_matcher,slv_iter);
      keep_going = false;
      if (sub_match) {
        //printf( "ZOM MATCH iter_st<%zu> iter_end<%zu> sub_st<%zu> sub_end<%zu>\n", //
        //        slv_iter->_start, slv_iter->_end, sub_match->_view->_start, sub_match->_view->_end );
        the_zom->_items.push_back(sub_match);
        keep_going       = slv_iter->_start < slv_iter->_end;
        if(keep_going){
            OrkAssert(slv_iter);
            slv_iter->_start = sub_match->_view->_end + 1;
        }
      }
      else{
            //printf( "ZOM NO_MATCH iter_st<%zu> iter_end<%zu>\n", //
            //        slv_iter->_start, slv_iter->_end );
      }
    }
    if (the_zom->_items.size()) {
      auto slv_out    = std::make_shared<ScannerLightView>(*input_slv);
      slv_out->_start = the_zom->_items.front()->_view->_start;
      slv_out->_end   = the_zom->_items.back()->_view->_end;
      the_match->_view = slv_out;
      return the_match;
    }
    else{
        auto slv_out    = std::make_shared<ScannerLightView>(*input_slv);
        slv_out->clear();
        the_match->_view = slv_out;
        return the_match;
    }
  };
  if(name==""){
     name = "zeroOrMore";
  }
  return createMatcher(match_fn,name);
}

//////////////////////////////////////////////////////////////////////

matcher_ptr_t Parser::createMatcher(matcher_fn_t match_fn,std::string name) {
  auto matcher = std::make_shared<Matcher>(match_fn);
  _matchers.insert(matcher);
  matcher->_name = name;
  return matcher;
}

//////////////////////////////////////////////////////////////

matcher_ptr_t Parser::matcherForTokenClassID(uint64_t tokclass,std::string name) {
  auto match_fn = [tokclass](matcher_ptr_t par_matcher, scannerlightview_constptr_t slv) -> match_ptr_t {
    auto slv_tokclass = slv->token(0)->_class;
    if (slv_tokclass == tokclass) {
      match_ptr_t the_match = std::make_shared<Match>();
      the_match->_matcher = par_matcher;
      auto slv_out  = std::make_shared<ScannerLightView>(*slv);
      slv_out->_end = slv_out->_start;
      the_match->_view = slv_out;
      return the_match;
    }
    return nullptr;
  };
  auto matcher = createMatcher(match_fn,name);
  matcher->_notif = [name](match_ptr_t the_match) {
    auto tok = the_match->_view->token(0);
    printf("MATCHED tok<%s> mname<%s>\n", tok->text.c_str(), name.c_str());
  };
  return matcher;
}

//////////////////////////////////////////////////////////////

matcher_ptr_t Parser::matcherForWord(std::string word) {
  auto match_fn = [word](matcher_ptr_t par_matcher, scannerlightview_constptr_t inp_view) -> match_ptr_t {
    auto tok0 = inp_view->token(0);
    if (tok0->text == word) {
      auto slv  = std::make_shared<ScannerLightView>(*inp_view);
      slv->_end = slv->_start;
      auto the_match = std::make_shared<Match>();
      the_match->_matcher = par_matcher;
      the_match->_view    = slv;
      return the_match;
    } else {
      return nullptr;
    }
  };
  auto matcher = createMatcher(match_fn,word);
  matcher->_notif = [word](match_ptr_t the_match) {
    printf("MATCHED word<%s>\n", word.c_str());
  };
  return matcher;
}

//////////////////////////////////////////////////////////////////////

match_ptr_t Parser::_match(matcher_ptr_t matcher, scannerlightview_constptr_t inp_view){
  _matcherstack.push(matcher);
  auto match = matcher->_match_fn(matcher,inp_view);
  if (match and matcher->_notif) {
    matcher->_notif(match);
  }
  _matcherstack.pop();
  return match;
}

//////////////////////////////////////////////////////////////

match_ptr_t Parser::match(scannerlightview_constptr_t inp_view, matcher_ptr_t top){
    return _match(top,inp_view);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork
/////////////////////////////////////////////////////////////////////////////////////////////////
