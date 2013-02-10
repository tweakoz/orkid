////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>

///////////////////////////////////////////////////////////////////////////////

#include <orktool/ged/ged.h>
#include <orktool/ged/ged_delegate.h>
#include <orktool/ged/ged_io.h>
#include <ork/reflect/IProperty.h>
#include <ork/reflect/IObjectProperty.h>
#include <ork/reflect/DirectObjectMapPropertyType.h>
#include <ork/reflect/IObjectPropertyObject.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/lev2/gfx/dbgfontman.h>

#include "ged_delegate_file.hpp"

typedef ork::tool::ged::GedIoDriver< ork::file::Path > FilePathObjDriverType;
typedef ork::tool::ged::GedFileNode<FilePathObjDriverType> GedFileNodeOfFilePath;
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool { namespace ged {
///////////////////////////////////////////////////////////////////////////////

void GedFactoryFileList::Describe() {}		

GedItemNode* GedFactoryFileList::CreateItemNode(ObjModel&mdl,const ConstString& Name,const reflect::IObjectProperty *prop,Object* obj) const
{
	GedFileNode<FilePathObjDriverType>* itemw = new GedFileNode<FilePathObjDriverType>( 
		mdl, 
		Name.c_str(),
		prop,
		obj
		);

	itemw->SetLabel();

	return itemw;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

} } }

INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(GedFileNodeOfFilePath,"GedFileNode");
INSTANTIATE_TRANSPARENT_RTTI( ork::tool::ged::GedFactoryFileList, "ged.factory.filelist" );

template class ork::tool::ged::GedFileNode<FilePathObjDriverType>;
