///////////////////////////////////////////////////////////////////////////////
//
//	Orkid QT User Interface Glue
//
///////////////////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>

///////////////////////////////////////////////////////////////////////////////

#include <orktool/manip/manip.h>
#include <ork/file/path.h>
#include <ork/file/tinyxml/tinyxml.h>
#include <ork/lev2/lev2_asset.h>
#include <orktool/toolcore/builtinchoices.h>

#include <QMenu>

namespace ork { namespace tool {

namespace ged {
///////////////////////////////////////////////////////////////////////////

struct StackNode {
  slashnode_constptr_t _node;
  QMenu* mpmenu;
  StackNode(slashnode_constptr_t pnode, QMenu* pmenu) {
    _node  = pnode;
    mpmenu = pmenu;
  }
};

///////////////////////////////////////////////////////////////////////////

QMenu* qmenuFromChoiceList(
    util::choicelist_ptr_t chclist, //
    util::choicefilter_ptr_t Filter) {
  QMenu* pMenu = new QMenu(0);

  auto hier = chclist->hierarchy();

  auto prootnode = hier->root();
  ;

  std::queue<StackNode> NodeStack;

  NodeStack.push(StackNode(prootnode, pMenu));

  while (false == NodeStack.empty()) {
    StackNode snode = NodeStack.front();

    NodeStack.pop();

    int inumchildren = snode._node->GetNumChildren();

    for (auto it : snode._node->GetChildren()) {
      auto pchild = it.second;

      bool IsASlash = (pchild->GetNodeName() == "/");

      if (pchild->IsLeaf()) {
        bool badd = true;

        if (Filter) {
          badd = false;

          if (Filter->mFilterMap.size()) {
            util::choice_constptr_t chcval = chclist->FindFromLongName(pchild->pathAsString());

            if (chcval) {
              for (orkmultimap<std::string, std::string>::const_iterator it = Filter->mFilterMap.begin();
                   it != Filter->mFilterMap.end();
                   it++) {
                const std::string& key = it->first;
                const std::string& val = it->second;

                badd |= chcval->HasKeyword(key + "=" + val);
              }
            }
          }
        }
        if (badd) {
          QAction* pchildact = snode.mpmenu->addAction(pchild->GetNodeName().c_str());

          util::choice_constptr_t chcval = chclist->FindFromLongName(pchild->pathAsString());

          pchildact->setProperty("chcval", QVariant::fromValue((void*)chcval.get()));

          QVariant UserData(QString(pchild->pathAsString().c_str()));

          pchildact->setData(UserData);
        }
      } else {
        if (chclist->DoesSlashNodePassFilter(pchild.get(), Filter)) {
          if (IsASlash) {
            NodeStack.push(StackNode(pchild, snode.mpmenu));
          } else {
            QMenu* pchildmenu = snode.mpmenu->addMenu(pchild->GetNodeName().c_str());
            NodeStack.push(StackNode(pchild, pchildmenu));
          }
        }
      }
    }
  }

  return pMenu;
}

} // namespace ged

///////////////////////////////////////////////////////////////////////////

#if 0 // defined(USE_FCOLLADA)
void ColladaChoiceCache(
    const file::Path& sdir,
    ChoiceList* ChcList,
    const std::string& ext,
    const ork::tool::meshutil::CColladaAsset::EAssetType TestType) {
  // TiXmlDeclaration * XmlDeclNode = new TiXmlDeclaration( "1.0", "", "" );
  // TiXmlElement *CacheRootNode = new TiXmlElement( CacheName );
  // CacheDoc.LinkEndChild( XmlDeclNode );
  // CacheDoc.LinkEndChild( CacheRootNode );
  // file::Path searchdir = FileEnv::GetRef().GetPathFromUrlExt("data://");
  file::Path::NameType wildcard         = CreateFormattedString("*.%s", ext.c_str()).c_str();
  orkvector<file::Path::NameType> files = FileEnv::filespec_search(wildcard, sdir);
  int inumfiles                         = (int)files.size();
  file::Path::NameType searchdir(sdir.ToAbsolute().c_str());
  searchdir.replace_in_place("\\", "/");
  for (int ifile = 0; ifile < inumfiles; ifile++) {
    file::Path::NameType ObjPtrStr = FileEnv::filespec_no_extension(FileEnv::filespec_strip_base(files[ifile], "./"));
    ObjPtrStr.replace_in_place(searchdir.c_str(), "");
    ChcList->add(AttrChoiceValue(ObjPtrStr.c_str(), ObjPtrStr.c_str()));
  }
}
#endif

///////////////////////////////////////////////////////////////////////////

void ModelChoices::EnumerateChoices(bool bforcenocache) {
  clear();
  // FindAssetChoices("data://", "*.xgm");
  auto items = lev2::XgmModelAsset::GetClassStatic()->EnumerateExisting();
  for (const auto& i : items)
    add(util::AttrChoiceValue(i.c_str(), i.c_str()));
  // ColladaChoiceCache( "data://", this, "xgm", CColladaAsset::ECOLLADA_MODEL
  // );
}

///////////////////////////////////////////////////////////////////////////

void AnimChoices::EnumerateChoices(bool bforcenocache) {
  clear();
#if defined(USE_FCOLLADA)
  // ColladaChoiceCache("data://", this, "xga", ork::tool::meshutil::CColladaAsset::ECOLLADA_ANIM);
#endif
}

///////////////////////////////////////////////////////////////////////////

void AudioStreamChoices::EnumerateChoices(bool bforcenocache) {
  clear();
  FindAssetChoices("data://", "*.xwma");
  // FindAssetChoices( "data://", "*.wav" );
  // FindAssetChoices( "data://", "*.mp3" );
}

///////////////////////////////////////////////////////////////////////////

void AudioBankChoices::EnumerateChoices(bool bforcenocache) {
  clear();
  FindAssetChoices("data://", "*.xab");
}

///////////////////////////////////////////////////////////////////////////

void ScriptChoices::EnumerateChoices(bool bforcenocache) {
  clear();
  FindAssetChoices("data://", "*.scr");
}

///////////////////////////////////////////////////////////////////////////

void TextureChoices::EnumerateChoices(bool bforcenocache) {
  clear();

  auto items = lev2::TextureAsset::GetClassStatic()->EnumerateExisting();

  for (const auto& i : items)
    add(util::AttrChoiceValue(i.c_str(), i.c_str()));
}

///////////////////////////////////////////////////////////////////////////

void FxShaderChoices::EnumerateChoices(bool bforcenocache) {
  clear();
  FindAssetChoices("data://", "*.fx");
}

///////////////////////////////////////////////////////////////////////////

void ChsmChoices::EnumerateChoices(bool bforcenocache) {
  clear();
  FindAssetChoices("data://chsms/", "*.mox");
}

///////////////////////////////////////////////////////////////////////////

ChsmChoices::ChsmChoices() {
  EnumerateChoices();
}

///////////////////////////////////////////////////////////////////////////

FxShaderChoices::FxShaderChoices() {
  EnumerateChoices();
}

AudioStreamChoices::AudioStreamChoices() {
  EnumerateChoices();
}

AudioBankChoices::AudioBankChoices() {
  EnumerateChoices();
}

ModelChoices::ModelChoices() {
  EnumerateChoices();
}

AnimChoices::AnimChoices() {
  EnumerateChoices();
}

TextureChoices::TextureChoices() {
  EnumerateChoices();
}
ScriptChoices::ScriptChoices() {
  EnumerateChoices();
}

}} // namespace ork::tool
