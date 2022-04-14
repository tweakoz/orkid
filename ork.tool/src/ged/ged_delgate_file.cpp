////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>

///////////////////////////////////////////////////////////////////////////////

#include <orktool/ged/ged.h>
#include <orktool/ged/ged_delegate.h>
#include <orktool/ged/ged_io.h>

#include <ork/reflect/properties/ObjectProperty.h>
#include <ork/reflect/properties/DirectTypedMap.h>
#include <ork/reflect/properties/IObject.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/lev2/gfx/dbgfontman.h>

#include "ged_delegate_file.hpp"

typedef ork::tool::ged::GedIoDriver<ork::file::Path> FilePathObjDriverType;
typedef ork::tool::ged::GedFileNode<FilePathObjDriverType> GedFileNodeOfFilePath;
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool { namespace ged {
///////////////////////////////////////////////////////////////////////////////

void GedFactoryFileList::Describe() {
}

GedItemNode* GedFactoryFileList::CreateItemNode(
    ObjModel& mdl, //
    const ConstString& Name,
    const reflect::ObjectProperty* prop,
    Object* obj) const {
  GedFileNode<FilePathObjDriverType>* itemw = new GedFileNode<FilePathObjDriverType>(mdl, Name.c_str(), prop, obj);

  itemw->SetLabel();

  return itemw;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}}} // namespace ork::tool::ged

INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(GedFileNodeOfFilePath, "GedFileNode");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ged::GedFactoryFileList, "ged.factory.filelist");

template class ork::tool::ged::GedFileNode<FilePathObjDriverType>;
