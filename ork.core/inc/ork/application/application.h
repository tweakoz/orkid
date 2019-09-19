///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orkstl.h>

#include <ork/kernel/core/singleton.h>
#include <ork/kernel/string/StringPool.h>
#include <ork/kernel/any.h>
#include <ork/util/Context.h>

#include <ork/rtti/RTTI.h>
#include <ork/object/Object.h>

namespace ork {

/**
 * Very high-level application code.
 */

class Application;

class Application : public Object
{
    RttiDeclareAbstract( Application, Object );

public:

	static PoolString AddPooledString(const PieceString &);
	static PoolString AddPooledLiteral(const ConstString &);
	static PoolString FindPooledString(const PieceString &);

	Application();

	static Application* GetContext() { return gctx; }

	StringPool &GetStringPool() { return mStringPool; }
	const StringPool &GetStringPool() const { return mStringPool; }

	void Init() { DoInit(); }
	bool PreUpdate() { return DoPreUpdate(); }
    void Update()   {   PreUpdate();
                        DoUpdate();
                        PostUpdate();
                    }
	void PostUpdate() { DoPostUpdate(); }
    void Draw() { DoDraw(); }

    anyp GetProperty( const std::string& key ) const;
    void SetProperty( const std::string& key, anyp val );

protected:

	virtual void DoInit() {}
	virtual bool DoPreUpdate() { return true; }
	virtual void DoUpdate() {}
	virtual void DoPostUpdate() {}
    virtual void DoDraw() {}

	StringPool mStringPool;
    orkmap<std::string,anyp> mAppPropertyMap;

private:

	static Application* gctx;

};


PoolString AddPooledString(const PieceString &ps);
PoolString AddPooledLiteral(const ConstString &cs);
PoolString FindPooledString(const PieceString &ps);

PoolString operator"" _pool(const char* s, size_t len);

}

typedef ork::util::GlobalStack<ork::Application*> ApplicationStack;
