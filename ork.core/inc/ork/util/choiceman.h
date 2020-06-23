///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/slashnode.h>
#include <ork/kernel/svariant.h>

#include <ork/file/path.h>

///////////////////////////////////////////////////////////////////////////////

// class QMenu;

namespace ork { namespace util {

struct ChoiceList;
struct ChoiceManager;
struct AttrChoiceValue;
struct ChoiceListFilters;

using ChoiceListFilter      = std::pair<std::string, std::string>;
using choice_ptr_t          = std::shared_ptr<AttrChoiceValue>;
using choicelist_ptr_t      = std::shared_ptr<ChoiceList>;
using choicemanager_ptr_t   = std::shared_ptr<ChoiceManager>;
using choicefilter_ptr_t    = std::shared_ptr<ChoiceListFilters>;
using choice_constptr_t     = std::shared_ptr<const AttrChoiceValue>;
using choicelist_constptr_t = std::shared_ptr<const ChoiceList>;

///////////////////////////////////////////////////////////////////////////////

struct ChoiceFunctor {
  virtual std::string ComputeValue(const std::string& ValueStr) const;
};

///////////////////////////////////////////////////////////////////////////////

struct AttrChoiceValue {

  typedef ork::svar128_t variant_t;

  AttrChoiceValue()
      : mName("defaultchoice")
      , mValue("")
      , mpSlashNode(0)
      , mShortName("")
      , mpFunctor(0) {
  }

  AttrChoiceValue(const std::string& nam, const std::string& val, const std::string& shname = "")
      : mName(nam)
      , mValue(val)
      , mShortName(shname)
      , mpFunctor(0) {
  }

  AttrChoiceValue set(const std::string& nname, const std::string& nval, const std::string& shname = "") {
    mName      = nname;
    mValue     = nval;
    mShortName = shname;
    return (*this);
  }

  std::string GetValue(void) const {
    return mValue;
  }
  std::string EvaluateValue(void) const {
    return (mpFunctor == 0) ? mValue : mpFunctor->ComputeValue(mValue);
  }
  void SetValue(const std::string& val) {
    mValue = val;
  }
  SlashNode* GetSlashNode(void) const {
    return mpSlashNode;
  }
  void SetSlashNode(SlashNode* pnode) {
    mpSlashNode = pnode;
  }

  const std::string GetName(void) const {
    return mName;
  }
  void SetName(const std::string& name) {
    mName = name;
  }
  const std::string shortname(void) const {
    return mShortName;
  }
  void SetShortName(const std::string& name) {
    mShortName = name;
  }

  void AddKeyword(const std::string& Keyword) {
    mKeywords.insert(Keyword);
  }

  bool HasKeyword(const std::string& Keyword) const {
    return mKeywords.find(Keyword) != mKeywords.end();
  }

  void CopyKeywords(const AttrChoiceValue& From) {
    mKeywords = From.mKeywords;
  }

  void SetFunctor(const ChoiceFunctor* ftor) {
    mpFunctor = ftor;
  }
  const ChoiceFunctor* GetFunctor(void) const {
    return mpFunctor;
  }

  void SetCustomData(const variant_t& data) {
    mCustomData = data;
  }
  const variant_t& GetCustomData() const {
    return mCustomData;
  }

  std::string mValue;
  std::string mName;
  std::string mShortName;
  SlashNode* mpSlashNode;
  orkset<std::string> mKeywords;
  const ChoiceFunctor* mpFunctor;
  variant_t mCustomData;
};

///////////////////////////////////////////////////////////////////////////////

struct ChoiceListFilters {
  orkmultimap<std::string, std::string> mFilterMap;

  void AddFilter(const std::string& key, const std::string& val) {
    mFilterMap.insert(ChoiceListFilter(key, val));
  }
  bool KeyMatch(const std::string& key, const std::string& val) const;
};

///////////////////////////////////////////////////////////////////////////////

struct ChoiceList {

  ChoiceList();
  virtual ~ChoiceList();

  void UpdateHierarchy(void);

  void remove(const AttrChoiceValue& val);
  void clear(void);

  void FindAssetChoices(const file::Path& sdir, const std::string& wildcard);

  choice_constptr_t FindFromLongName(const std::string& longname) const;
  choice_constptr_t FindFromShortName(const std::string& shortname) const;
  choice_constptr_t FindFromValue(const std::string& uval) const;

  slashtree_constptr_t hierarchy(void) const {
    return _hierarchy;
  }

  virtual void EnumerateChoices(bool bforcenocache = false) {
  }
  void add(const AttrChoiceValue& val);

  bool DoesSlashNodePassFilter(
      const SlashNode* pnode, //
      choicefilter_ptr_t Filter) const;

  void dump(void);

  orkvector<choice_ptr_t> mChoicesVect;
  orkmap<std::string, choice_ptr_t> mValueMap;
  orkmap<std::string, choice_ptr_t> mNameMap;
  orkmap<std::string, choice_ptr_t> mShortNameMap;
  slashtree_ptr_t _hierarchy;
};

///////////////////////////////////////////////////////////////////////////////

struct ChoiceManager {

  void AddChoiceList(
      const std::string& ListName, //
      choicelist_ptr_t plist);
  choicelist_constptr_t GetChoiceList(const std::string& ListName) const;
  choicelist_ptr_t GetChoiceList(const std::string& ListName);

  ChoiceManager();
  ~ChoiceManager();

  orkmap<std::string, choicelist_ptr_t> _choicelists;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::util

///////////////////////////////////////////////////////////////////////////////
