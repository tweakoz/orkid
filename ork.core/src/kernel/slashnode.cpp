////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/slashnode.h>
#include <ork/file/path.h>

namespace ork {

///////////////////////////////////////////////////////////////////////////////
// slashnode
//	separate a std::string in the form "this/is/a/slash/path"
//	into subpaths "this" "is" "a" "slash" "path"
//	note that spaces are legal!
///////////////////////////////////////////////////////////////////////////////

std::string::size_type str_cue_to_char(const std::string& str, char cch, int start) {
  std::string::size_type slen = str.size();
  bool found                  = false;
  std::string::size_type idx  = std::string::size_type(start);
  std::string::size_type rval = std::string::npos;
  for (std::string::size_type i = idx; ((i < slen) && (found == false)); i++) {
    if (cch == str[i]) {
      found = true;
      rval  = i;
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void breakup_slash_path(std::string str, orkvector<std::string>& outvec) {
  file::Path AsPath(str.c_str());

  outvec.clear();
  if (AsPath.hasUrlBase()) {
    outvec.push_back(AsPath.getUrlBase().c_str());
  }
  if (AsPath.hasDrive()) {
    outvec.push_back(AsPath.getDrive().c_str());
  }
  if (AsPath.hasFolder()) {
    const std::string str2 = AsPath.getFolder(ork::file::Path::EPATHTYPE_POSIX).c_str();

    const std::string delims("/");
    int word                    = 0;
    bool bDone                  = false;
    std::string::size_type ilen = str2.size();
    std::string::size_type idx  = 0;
    while ((idx < ilen) && (!bDone)) {
      if (str2[idx] == '/') {
        // outvec.push_back("/");
        idx++;
      } else {
        std::string::size_type Nidx = str_cue_to_char(str2, '/', int(idx + 1));
        bool bAnotherSlash          = (Nidx != -1);
        std::string::size_type len  = Nidx - idx;
        std::string newword         = str2.substr(idx, len);

        if (newword.length()) {
          outvec.push_back(newword);
        }
        idx = Nidx;
        word++;
      }
    }
  }
  if (AsPath.hasFile()) {
    outvec.push_back(AsPath.getName().c_str());
  }
  if (AsPath.hasExtension()) {
    outvec[outvec.size() - 1] += '.';
    outvec[outvec.size() - 1] += AsPath.getExtension().c_str();
  }
}

///////////////////////////////////////////////////////////////////////////////

SlashNode::~SlashNode() {
}

///////////////////////////////////////////////////////////////////////////////

SlashNode::SlashNode()
    : _name("default")
    , _parent(nullptr)
    , _data(nullptr) {
}

///////////////////////////////////////////////////////////////////////////////

void SlashNode::dump(void) const {
  printf("Node<%p>: ", (void*) this);
  _dump();
}

void SlashNode::_dump(void) const {
  orkvector<std::string> path_vect;

  const SlashNode* par = this;

  while (par != 0) {
    path_vect.push_back(par->_name);
    par = par->_parent;
  }

  size_t npel = path_vect.size();
  for (size_t i = 0; i < npel; i++) {
    size_t j  = npel - (i + 1);
    char* str = (char*)path_vect[j].c_str();
    if (strlen(str) != 0)
      printf("/%s", str);
  }

  printf("\n");

  for (auto it : _children_map)
    it.second->_dump();
}

///////////////////////////////////////////////////////////////////////////////

const SlashNode* SlashNode::root() const {
  return _parent ? _parent->root() : this;
}

///////////////////////////////////////////////////////////////////////////////

void SlashNode::add_child(slashnode_ptr_t child) {
  _children_map[child->_name] = child;
  child->_parent              = this;
}

///////////////////////////////////////////////////////////////////////////////

std::string SlashNode::pathAsString() const {

  if (this == root()) {
    return "/";
  }

  orkvector<const SlashNode*> hier;
  GetPath(hier);
  int inumnodes = int(hier.size());
  std::string rval;
  for (int i = 0; i < inumnodes; i++) {
    if (hier[i] != root()) {
      rval += "/" + hier[i]->_name;
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void SlashNode::GetPath(orkvector<const SlashNode*>& pth) const {
  orkvector<const SlashNode*> revpth;
  const SlashNode* cur = this;
  while (cur) {
    revpth.push_back(cur);
    cur = cur->_parent;
  }
  int inumnodes = int(revpth.size());
  pth.resize(inumnodes);
  for (int i = 0; i < inumnodes; i++) {
    pth[i] = revpth[(inumnodes - 1) - i];
  }
}

///////////////////////////////////////////////////////////////////////////////

void SlashTree::remove_node(SlashNode* pnode) {
  bool bremoved = false;

  auto pparent = pnode->_parent;

  if (pparent) {

    for (auto it : pparent->_children_map) {
      slashnode_ptr_t tnode = it.second;

      if (tnode.get() == pnode) {
        OldStlSchoolRemoveFromMap(pparent->_children_map, pnode->_name);
        bremoved = true;
        break;
      }
    }

    if (bremoved) {
      int inumchildren = int(pparent->_children_map.size());

      if (0 == inumchildren) {
        void* pdata = pparent->_data;

        remove_node(pparent);
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

slashnode_ptr_t SlashTree::add_node(const char* instr, void* ndata) {
  slashnode_ptr_t rval = 0;

  orkvector<std::string> parsed_path;
  breakup_slash_path(instr, parsed_path);

  ///////////////////////////////////
  //	search for deepest node found

  bool found = true;

  size_t Nppath       = parsed_path.size();
  slashnode_ptr_t ptr = _root;

  for (size_t idx = 0; idx < Nppath; idx++) {
    std::string mstr                                  = parsed_path[idx];
    orkmap<std::string, slashnode_ptr_t>::iterator it = ptr->_children_map.find(mstr);
    if (it != ptr->_children_map.end()) {
      std::pair<const std::string, slashnode_ptr_t>* pair = &(*it);
      ptr                                                 = pair->second;
    } else // not found
    {
      auto nnod                = std::make_shared<SlashNode>();
      nnod->_name              = mstr;
      ptr->_children_map[mstr] = nnod;
      nnod->_parent            = ptr.get();
      rval                     = nnod;

      // orkprintf( "added node %08x %s parent %08x %s (Data %08x)\n", nnod, mstr.c_str(), ptr, ptr->_name.c_str(), ndata );

      nnod->_data = ndata;
      ptr         = nnod;
    }
  }

  /////////////////////

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void SlashTree::Clear(void) {
  _root        = std::make_shared<SlashNode>();
  _root->_name = "";
}

///////////////////////////////////////////////////////////////////////////////

SlashTree::SlashTree()
    : _root(nullptr) {
  Clear();
}

///////////////////////////////////////////////////////////////////////////////

void SlashTree::dump(void) const {
  orkprintf("////////////////////////////////////////\n");
  orkprintf("dumping slashnode hierarchy %p\n", this);
  _root->dump();
  orkprintf("////////////////////////////////////////\n");
}

} // namespace ork
