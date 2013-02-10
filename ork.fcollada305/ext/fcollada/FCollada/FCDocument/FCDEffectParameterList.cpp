/*
    Copyright (C) 2005-2007 Feeling Software Inc.
    MIT License: http://www.opensource.org/licenses/mit-license.php
*/
#if 0

#include "StdAfx.h"
#include "FCDocument/FCDEffectParameter.h"
#include "FCDocument/FCDEffectParameterFactory.h"
#include "FCDocument/FCDEffectParameterList.h"

ImplementObjectType(FCDEffectParameterList);

FCDEffectParameterList::FCDEffectParameterList(FCDocument* document, bool _ownParameters)
:	FCDObject(document)
,	ownParameters(_ownParameters)
{
}

FCDEffectParameterList::~FCDEffectParameterList()
{
	if (ownParameters)
	{
		while (!parameters.empty())
		{
			parameters.back()->Release();
		}
	}
	parameters.clear();
	ownParameters = false;
}

FCDEffectParameter* FCDEffectParameterList::AddParameter(uint32 type)
{
	FCDEffectParameter* parameter = NULL;
	if (ownParameters)
	{
		parameter = FCDEffectParameterFactory::Create(GetDocument(), type);
		parameters.push_back(parameter);
	}
	SetDirtyFlag();
	return parameter;
}

const FCDEffectParameter* FCDEffectParameterList::FindReference(const char* reference) const
{
	for (FCDEffectParameterTrackList::const_iterator it = begin(); it != end(); ++it)
	{
		if ((*it)->GetReference() == reference) return (*it);
	}
	return NULL;
}

const FCDEffectParameter* FCDEffectParameterList::FindSemantic(const char* semantic) const
{
	for (FCDEffectParameterTrackList::const_iterator it = begin(); it != end(); ++it)
	{
		if ((*it)->GetSemantic() == semantic) return (*it);
	}
	return NULL;
}

void FCDEffectParameterList::FindReference(const char* reference, FCDEffectParameterList& list)
{
	for (FCDEffectParameterTrackList::iterator it = begin(); it != end(); ++it)
	{
		if ((*it)->GetReference() == reference) list.AddParameter(*it);
	}
}

void FCDEffectParameterList::FindSemantic(const char* semantic, FCDEffectParameterList& list)
{
	for (FCDEffectParameterTrackList::iterator it = begin(); it != end(); ++it)
	{
		if ((*it)->GetSemantic() == semantic) list.AddParameter(*it);
	}
}

void FCDEffectParameterList::FindType(uint32 type, FCDEffectParameterList& list) const
{
	for (FCDEffectParameterTrackList::const_iterator it = begin(); it != end(); ++it)
	{
		if ((*it)->GetType() == (FCDEffectParameter::Type) type) list.AddParameter(const_cast<FCDEffectParameter*>(*it));
	}
}

// Copy this list
FCDEffectParameterList* FCDEffectParameterList::Clone(FCDEffectParameterList* clone) const
{
	if (clone == NULL)
	{
		clone = new FCDEffectParameterList(const_cast<FCDocument*>(GetDocument()), ownParameters);
	}

	if (!parameters.empty())
	{
		clone->parameters.reserve(parameters.size());

		if (ownParameters)
		{
			for (FCDEffectParameterTrackList::const_iterator it = begin(); it != end(); ++it)
			{
				FCDEffectParameter* clonedParam = FCDEffectParameterFactory::Create(clone->GetDocument(), (*it)->GetType());
				clone->parameters.push_back((*it)->Clone(clonedParam));
			}
		}
		else
		{
			(*clone) = (*this);
		}
	}
	return clone;
#endif}
