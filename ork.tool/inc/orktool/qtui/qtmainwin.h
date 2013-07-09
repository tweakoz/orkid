////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _ORK_TOOL_QTMAINWIN_H 
#define _ORK_TOOL_QTMAINWIN_H 

///////////////////////////////////////////////////////////////////////////

#include <ork/kernel/slashnode.h>

///////////////////////////////////////////////////////////////////////////

namespace ork {

class HotKeyManager;

namespace dataflow {
class graph;
class dgmodule;
}

namespace tool {

///////////////////////////////////////////////////////////////////////////

class EditorModule : public QObject
{
	DeclareMoc( EditorModule, QObject );
	
protected:

	SlashTree						mSlashHier;

	EditorModule();
	QMenuBar*	mMenuBar;
	void AddAction( QMenu* pmenu, const char* pname );
	void AddAction( const char* pname);
	void AddAction(   const char* pname,QKeySequence ks);

public:

	virtual void Activate( QMenuBar* qmb ) {}
	virtual void DeActivate( QMenuBar* qmb ) {}
	virtual void OnAction( const char* pact ) {}
	void ActionSlot();
};

///////////////////////////////////////////////////////////////////////////

class EditorModuleMgr	
{
	QMenuBar*							mMenuBar;
	orkmap<std::string,EditorModule*>	mModules;
	SlashTree						mSlashHier;

public:
	EditorModuleMgr(QMenuBar*mb) : mMenuBar(mb) {}
	void AddModule( std::string name, EditorModule*mod );
	void RemoveModule( std::string name );
};

///////////////////////////////////////////////////////////////////////////

/// Common window class for any Miniork window (both editor and game) that displays information on the title bar
class MiniorkMainWindow : public QMainWindow
{
	DeclareMoc( MiniorkMainWindow, QMainWindow );

public:

	MiniorkMainWindow(QWidget* parent = 0, Qt::WFlags flags = 0);

	const QString& GetMainTitle() const;
	void SetMainTitle(const QString& str);

	const QString& GetMiniorkRepos() const;
	void SetMiniorkRepos(const QString& str);

	const QString& GetMiniorkRev() const;
	void SetMiniorkRev(const QString& str);

	const QString& GetGameRepos() const;
	void SetGameRepos(const QString& str);

	const QString& GetGameRev() const;
	void SetGameRev(const QString& str);

	const QString& GetSceneFile() const;
	void SetSceneFile(const QString& str);

	bool IsReadOnly() const;
	void SetReadOnly(bool value);

	///////////////////////////////////////////////////////////////////////////

	EditorModuleMgr& ModuleMgr() { return mEditorModuleMgr; }

	///////////////////////////////////////////////////////////////////////////

	void FunctorAction();
	void CreateFunctionMenu(QMenu *menu, const std::string &name, ork::reflect::IFunctor *functor);
	void CreateFunctionMenu(const std::string &name, ork::reflect::IFunctor *functor);
	void CreateFunctionMenus();

	///////////////////////////////////////////////////////////////////////////

private:

	void UpdateTitle();

	QString mMainTitle;
	QString mMiniorkRepos;
	QString mMiniorkRev;
	QString mGameRepos;
	QString mGameRev;
	QString mSceneFile;
	bool mReadOnly;

protected:

	EditorModuleMgr	mEditorModuleMgr;
};

///////////////////////////////////////////////////////////////////////////

class QQedRefreshEvent : public QEvent
{
public:
	static const QEvent::Type gevtype = QEvent::Type(QEvent::User+0);
	QQedRefreshEvent();
};

///////////////////////////////////////////////////////////////////////////

} // namespace tool
} // namespace ork

///////////////////////////////////////////////////////////////////////////

#endif // _ORK_TOOL_QTMAINWIN_H
