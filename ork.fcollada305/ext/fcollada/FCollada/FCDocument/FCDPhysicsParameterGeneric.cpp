/*
    Copyright (C) 2005-2007 Feeling Software Inc.
    MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "FCDocument/FCDPhysicsParameterGeneric.h"
#include "FCDocument/FCDocument.h"

//
// FCDPhysicsParameterGeneric
//

ImplementObjectType(FCDPhysicsParameterGeneric);

FCDPhysicsParameterGeneric::FCDPhysicsParameterGeneric(FCDocument* document, const fm::string& ref)
:	FCDObject(document)
{
	isGenerator = true;
	reference = ref;
}

FCDPhysicsParameterGeneric::~FCDPhysicsParameterGeneric()
{
}

