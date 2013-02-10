/*
    Copyright (C) 2005-2007 Feeling Software Inc.
    MIT License: http://www.opensource.org/licenses/mit-license.php
*/
#if 0

#include "StdAfx.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDEntityInstance.h"
#include "FCDocument/FCDExternalReference.h"
#include "FCDocument/FCDExternalReferenceManager.h"
#include "FCDocument/FCDPlaceHolder.h"
#include "FUtils/FUFileManager.h"
#include "FUtils/FUXmlWriter.h"
//
// FCDExternalReference
//

ImplementObjectType(FCDExternalReference);

FCDExternalReference::FCDExternalReference(FCDocument* document, FCDEntityInstance* _instance)
:	FCDObject(document)
,	instance(_instance), placeHolder(NULL)
{
	if (instance != NULL)
	{
		TrackObject(instance);
	}
}

FCDExternalReference::~FCDExternalReference()
{
	if (instance != NULL)
	{
		instance->SetEntity(NULL);
		UntrackObject(instance);
		instance = NULL;
	}
	if (placeHolder != NULL)
	{
		placeHolder->RemoveExternalReference(this);
		UntrackObject(placeHolder);
		placeHolder = NULL;
	}
}

FUUri FCDExternalReference::GetUri(bool relative) const
{
	FUUri uri;
	if (placeHolder != NULL)
	{
		uri.prefix = GetDocument()->GetFileManager()->GetFileURL(placeHolder->GetFileUrl(), relative);
		FUXmlWriter::ConvertFilename(uri.prefix);
	}
	uri.suffix = entityId;
	return uri;
}

void FCDExternalReference::SetEntityDocument(FCDocument* document)
{
	FCDPlaceHolder* t = GetDocument()->GetExternalReferenceManager()->FindPlaceHolder(document);
	if (t == NULL)
	{
		t = GetDocument()->GetExternalReferenceManager()->AddPlaceHolder(document);
	}
	SetPlaceHolder(t);
	SetDirtyFlag();
}

void FCDExternalReference::SetUri(const FUUri& uri)
{
	fstring fileUrl = GetDocument()->GetFileManager()->GetFileURL(uri.prefix, false);
	entityId = uri.suffix;

	FCDPlaceHolder* t = GetDocument()->GetExternalReferenceManager()->FindPlaceHolder(fileUrl);
	if (t == NULL)
	{
		t = GetDocument()->GetExternalReferenceManager()->AddPlaceHolder(fileUrl);
	}
	SetPlaceHolder(t);
	SetDirtyFlag();
}

void FCDExternalReference::SetPlaceHolder(FCDPlaceHolder* _placeHolder)
{
	if (_placeHolder != placeHolder)
	{
		if (placeHolder != NULL)
		{
			placeHolder->RemoveExternalReference(this);
			UntrackObject(placeHolder);
		}
		placeHolder = _placeHolder;
		if (placeHolder != NULL)
		{
			placeHolder->AddExternalReference(this);
			TrackObject(placeHolder);
		}
		SetDirtyFlag();
	}
}

void FCDExternalReference::OnObjectReleased(FUObject* object)
{
	if (instance == object) instance = NULL;
	if (placeHolder == object) placeHolder = NULL;
	Release();
}

#endif

