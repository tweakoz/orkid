///////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////

#include <ork/kernel/string/string.h>
#include <ork/util/choiceman.h>
#include <ork/file/fileenv.h>
#include <ork/util/stl_ext.h>

namespace ork { namespace util {

std::string ChoiceFunctor::ComputeValue(const std::string& ValueStr) const {
  return ValueStr;
}

///////////////////////////////////////////////////////////////////////////////

ChoiceList::ChoiceList()
    : _hierarchy(new SlashTree) {
}

///////////////////////////////////////////////////////////////////////////////

ChoiceList::~ChoiceList() {
}

///////////////////////////////////////////////////////////////////////////////

choice_constptr_t ChoiceList::FindFromLongName(const std::string& longname) const {
  choice_ptr_t rval = OldStlSchoolFindValFromKey(mNameMap, longname, choice_ptr_t(nullptr));
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

choice_constptr_t ChoiceList::FindFromShortName(const std::string& shortname) const {
  choice_ptr_t rval = OldStlSchoolFindValFromKey(mShortNameMap, shortname, choice_ptr_t(nullptr));
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

choice_constptr_t ChoiceList::FindFromValue(const std::string& uval) const {
  choice_ptr_t rval = OldStlSchoolFindValFromKey(mValueMap, uval, choice_ptr_t(nullptr));
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void ChoiceList::add(const AttrChoiceValue& val) {
  std::string LongName = val.GetName();

  //////////////////////////////////////
  // insert leading slash if not present

  file::Path apath(LongName.c_str());

  if (apath.HasUrlBase() == false) {
    char ch0 = val.GetName().c_str()[0];

    if (ch0 != '/') {
      LongName = CreateFormattedString("/%s", LongName.c_str());
    }
  }

  //////////////////////////////////////

  auto pNewVal = std::make_shared<AttrChoiceValue>(LongName, val.GetValue(), val.GetShortName());
  pNewVal->SetFunctor(val.GetFunctor());
  mChoicesVect.push_back(pNewVal);
  int nch               = (int)mChoicesVect.size() - 1;
  auto ChcVal           = mChoicesVect[nch];
  void* pData           = (void*)ChcVal.get();
  slashnode_ptr_t pnode = _hierarchy->add_node(LongName.c_str(), pData);
  pNewVal->SetSlashNode(pnode.get());
  pNewVal->CopyKeywords(val);
  pNewVal->SetCustomData(val.GetCustomData());

  OldStlSchoolMapInsert(mNameMap, LongName, pNewVal);
  OldStlSchoolMapInsert(mShortNameMap, val.GetShortName(), pNewVal);
  OldStlSchoolMapInsert(mValueMap, val.GetValue(), pNewVal);
}

///////////////////////////////////////////////////////////////////////////////

void ChoiceList::remove(const AttrChoiceValue& val) {
  int inumchoices = (int)mChoicesVect.size();

  for (int iv = 0; iv < inumchoices; iv++) {
    auto ChcVal = mChoicesVect[iv];

    if ((ChcVal->GetName() == val.GetName()) && (ChcVal->GetShortName() == val.GetShortName())) {
      SlashNode* pnode = ChcVal->GetSlashNode();

      if (pnode) {
        _hierarchy->remove_node(pnode);
      }

      inumchoices = (int)mChoicesVect.size();

      mChoicesVect[iv] = mChoicesVect[inumchoices - 1];
      mChoicesVect.pop_back();
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void ChoiceList::UpdateHierarchy(void) // update hierarchy
{
  _hierarchy = std::make_shared<SlashTree>();
  mValueMap.clear();

  size_t nch = mChoicesVect.size();
  for (size_t i = 0; i < nch; i++) {
    auto val = mChoicesVect[i];
    _hierarchy->add_node(val->GetName().c_str(), (void*)val.get());
    OldStlSchoolMapInsert(mValueMap, val->GetValue(), val);
  }
}

///////////////////////////////////////////////////////////////////////////////

void ChoiceList::dump(void) {
  _hierarchy->dump();
}

///////////////////////////////////////////////////////////////////////////////

void ChoiceList::clear(void) {
  _hierarchy = std::make_shared<SlashTree>();
  mChoicesVect.clear();

  mValueMap.clear();
  mNameMap.clear();
  mShortNameMap.clear();
}

///////////////////////////////////////////////////////////////////////////

bool ChoiceList::DoesSlashNodePassFilter(
    const SlashNode* pnode, //
    choicefilter_ptr_t Filter) const {
  bool bpass = true;

  if (Filter) {
    bpass = false;

    std::stack<const SlashNode*> NodeStack;

    NodeStack.push(pnode);

    while (false == NodeStack.empty()) {
      const SlashNode* snode = NodeStack.top();

      NodeStack.pop();

      int inumchildren = snode->GetNumChildren();

      auto children = snode->GetChildren();

      for (auto it : children) {

        auto pchild = it.second;

        if (pchild->IsLeaf()) {
          if (Filter) {
            if (Filter->mFilterMap.size()) {
              choice_constptr_t chcval = FindFromLongName(pchild->getfullpath());

              if (chcval) {
                for (orkmultimap<std::string, std::string>::const_iterator it = Filter->mFilterMap.begin();
                     it != Filter->mFilterMap.end();
                     it++) {
                  const std::string& key = it->first;
                  const std::string& val = it->second;

                  bpass |= chcval->HasKeyword(key + "=" + val);
                }
              }
            }
          }
        } else {
          NodeStack.push(pchild.get());
        }
      }
    }
  }
  return bpass;
}

///////////////////////////////////////////////////////////////////////////

void ChoiceList::FindAssetChoices(const file::Path& sdir, const std::string& wildcard) {
  orkvector<file::Path::NameType> files = FileEnv::filespec_search(wildcard.c_str(), sdir.c_str());
  int inumfiles                         = (int)files.size();
  file::Path::NameType searchdir(sdir.ToAbsolute().c_str());
  searchdir.replace_in_place("\\", "/");
  for (int ifile = 0; ifile < inumfiles; ifile++) {
    auto the_file                  = files[ifile];
    auto the_stripped              = FileEnv::filespec_strip_base(the_file, "./");
    file::Path::NameType ObjPtrStr = FileEnv::filespec_no_extension(the_stripped);
    file::Path::NameType ObjPtrStrA;
    ObjPtrStrA.replace(ObjPtrStr.c_str(), searchdir.c_str(), "");
    // OldStlSchoolFindAndReplace( ObjPtrStrA, searchdir, file::Path::NameType("") );
    file::Path::NameType ObjPtrStr2 = file::Path::NameType(sdir.c_str()) + ObjPtrStrA;
    file::Path OutPath(ObjPtrStr2.c_str());
    // OutPath.SetUrlBase( sdir.GetUrlBase().c_str() );
    add(AttrChoiceValue(OutPath.c_str(), OutPath.c_str()));
    // orkprintf( "FOUND ASSERT<%s>\n", the_file.c_str() );
  }
}

///////////////////////////////////////////////////////////////////////////////

ChoiceManager::ChoiceManager() {
}

///////////////////////////////////////////////////////////////////////////////

ChoiceManager::~ChoiceManager() {
}

///////////////////////////////////////////////////////////////////////////////

void ChoiceManager::AddChoiceList(
    const std::string& ListName, //
    choicelist_ptr_t plist) {
  auto it                 = _choicelists.find(ListName);
  choicelist_ptr_t plist2 = (it == _choicelists.end()) ? nullptr : it->second;
  OrkAssert(nullptr == plist2);
  if (nullptr == plist2) {
    OldStlSchoolMapInsert(_choicelists, ListName, plist);
  }
}

///////////////////////////////////////////////////////////////////////////////

choicelist_ptr_t ChoiceManager::GetChoiceList(const std::string& ListName) {
  choicelist_ptr_t plist = nullptr;
  auto it                = _choicelists.find(ListName);
  if (it != _choicelists.end()) {
    plist = it->second;
  }
  return plist;
}

///////////////////////////////////////////////////////////////////////////////

choicelist_constptr_t ChoiceManager::GetChoiceList(const std::string& ListName) const {
  choicelist_constptr_t plist = nullptr;
  auto it                     = _choicelists.find(ListName);
  if (it != _choicelists.end()) {
    plist = it->second;
  }
  return plist;
}

}} // namespace ork::util
