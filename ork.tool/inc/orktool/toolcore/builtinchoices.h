////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

namespace ork { namespace tool {
///////////////////////////////////////////////////////////////////////////

class CModelChoices : public ork::tool::CChoiceList
{
public:

	virtual void EnumerateChoices( bool bforcenocache=false );
	CModelChoices();
};

///////////////////////////////////////////////////////////////////////////

class CAnimChoices : public ork::tool::CChoiceList
{
public:

	virtual void EnumerateChoices( bool bforcenocache=false );
	CAnimChoices();
};

///////////////////////////////////////////////////////////////////////////

class AudioStreamChoices : public ork::tool::CChoiceList
{
public:

	virtual void EnumerateChoices( bool bforcenocache=false );
	AudioStreamChoices();
};

///////////////////////////////////////////////////////////////////////////

class AudioBankChoices : public ork::tool::CChoiceList
{
public:

	virtual void EnumerateChoices( bool bforcenocache=false );
	AudioBankChoices();
};

///////////////////////////////////////////////////////////////////////////

class CTextureChoices : public ork::tool::CChoiceList
{
public:

	virtual void EnumerateChoices( bool bforcenocache=false );
	CTextureChoices();
};

///////////////////////////////////////////////////////////////////////////

class FxShaderChoices : public ork::tool::CChoiceList
{
public:

	virtual void EnumerateChoices( bool bforcenocache=false );
	FxShaderChoices();
};

///////////////////////////////////////////////////////////////////////////

class ScriptChoices : public ork::tool::CChoiceList
{
public:

	virtual void EnumerateChoices( bool bforcenocache=false );
	ScriptChoices();
};

///////////////////////////////////////////////////////////////////////////

class ChsmChoices : public ork::tool::CChoiceList
{
public:

	virtual void EnumerateChoices( bool bforcenocache=false );
	ChsmChoices();
};

///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////
