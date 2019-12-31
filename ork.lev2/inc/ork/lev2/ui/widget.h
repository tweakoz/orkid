#pragma once

#include <ork/lev2/ui/event.h>
#include <functional>

namespace ork { namespace ui {

struct Group;

struct IWidgetEventFilter
{
	IWidgetEventFilter(Widget&w);
	void Filter( const Event& Ev );

	virtual void DoFilter(const Event& Ev) = 0;

	Widget& mWidget;
	bool mShiftDown;
	bool mCtrlDown;
	bool mMetaDown;
	bool mAltDown;
	bool mLeftDown;
	bool mMiddleDown;
	bool mRightDown;
	bool mCapsDown;
	bool mBut0Down;
	bool mBut1Down;
	bool mBut2Down;
	int mLastKeyCode;
	std::vector<int> mKeySequence;
	Timer mKeyTimer;
	Timer mDoubleTimer;
	Timer mMoveTimer;

};

struct WidgetEventFilter1 : public IWidgetEventFilter
{
	WidgetEventFilter1(Widget& w) : IWidgetEventFilter(w) {}
	void DoFilter( const Event& Ev );
};

struct Widget : public ork::Object
{
	RttiDeclareAbstract( Widget, ork::Object );

	friend struct Group;

public:

	static const int keycode_shift = 16777248; //Qt::Key_Shift;
	static const int keycode_cmd = 16777249;
	static const int keycode_alt = 16777251;
	static const int keycode_ctrl = 16777250;

	Widget( const std::string & name, int x, int y, int w, int h );
	~Widget();
	
	void Init( lev2::Context* pTARG );

	const std::string & GetName( void ) const { return msName; }
	void SetName( const std::string & name ) { msName = name; }

	lev2::Context* GetTarget( void ) const { return mpTarget; }

	int GetX( void ) const { return miX; }
	int GetY( void ) const  { return miY; }
	int GetX2( void ) const  { return miX2; }
	int GetY2( void ) const  { return miY2; }
	int GetW( void ) const  { return miW; }
	int GetH( void ) const  { return miH; }
	void SetX( int X ) { SetPos( X, miY ); }
	void SetY( int Y ) { SetPos( miX, Y ); }
	void SetX2( int X2 ) { SetSize( (X2-miX), miH ); }
	void SetY2( int Y2 ) { SetSize( miW, (Y2-miY) ); }
	void SetW( int W ) { SetSize( W, miH ); }
	void SetH( int H ) { SetSize( miW, H ); }

	void LocalToRoot(int lx, int ly, int& rx, int& ry) const;
	void RootToLocal(int rx, int ry, int& lx, int& ly) const;

	void SetPos( int iX, int iY );
	void SetSize( int iW, int iH );
	void SetRect( int iX, int iY, int iW, int iH );

	virtual void Show( void ) {}
	virtual void Hide( void ) {}

	void ExtDraw( lev2::Context* pTARG );
	virtual void Draw( DrawEvent& drq );

	virtual void Enter( void ) { /*mWidgetFlags.SetState( EUI_WIDGET_HOVER );*/ }
	virtual void Exit( void ) { /*mWidgetFlags.SetState( EUI_WIDGET_OFF );*/ }

	void GotKeyboardFocus( void ) { mbKeyboardFocus=true; }
	void LostKeyboardFocus( void ) { mbKeyboardFocus=false; }
	bool HasKeyboardFocus( void ) const { return mbKeyboardFocus; }

	bool IsKeyDepressed( int ch );
	bool IsHotKeyDepressed( const char* pact );
	bool IsHotKeyDepressed( const HotKey& hk );

	HandlerResult HandleUiEvent( const Event& Ev );

	bool IsEventInside( const Event& Ev ) const;

	void SetDirty();
	bool IsDirty() const { return mDirty; }
	Group* GetParent() const { return mParent; }
	//CWidgetFlags &GetFlagsRef( void ) { return mWidgetFlags; }

	virtual void OnResize( void );
	HandlerResult OnUiEvent( const Event& Ev );

	bool HasMouseFocus() const;
	void SetParent(Group*p) { mParent=p; }

	HandlerResult RouteUiEvent( const Event& Ev );

	float logicalWidth() const;
    float logicalHeight() const;
    float logicalX() const;
    float logicalY() const;

protected:

	bool				mbInit;
	bool				mbKeyboardFocus;
	int					miX, miY, miW, miH, miX2, miY2;		
	int					miLX, miLY, miLW, miLH, miLX2, miLY2;
	std::string			msName;								
	lev2::Context*	mpTarget;
	DrawEvent*			mpDrawEvent;
	bool				mDirty;
	bool				mSizeDirty;
	bool				mPosDirty;
	Group*				mParent;						
	std::stack<IWidgetEventFilter*> mEventFilterStack;

	static void UpdateMouseFocus(const HandlerResult& e, const Event& Ev);

private:

	virtual void DoInit( lev2::Context* pTARG ) {}
	virtual void DoOnEnter() {}
	virtual void DoOnExit() {}
	virtual void DoDraw(ui::DrawEvent& drwev) = 0;
	virtual HandlerResult DoOnUiEvent( const Event& Ev );
	void ReLayout();
	virtual void DoLayout() {}
	virtual HandlerResult DoRouteUiEvent( const Event& Ev );
};

////////////////////////////////////////////////////////////////////
// Group : abstract collection of widgets
////////////////////////////////////////////////////////////////////

struct Group : public Widget
{
public:

	Group( const std::string & name, int x, int y, int w, int h );

	void AddChild( Widget* pch );
	void RemoveChild( Widget* pch );

	void DrawChildren(ui::DrawEvent& drwev);

protected:

	std::vector<Widget*> mChildren;
	HandlerResult DoRouteUiEvent( const Event& Ev ) override;

private:

	void OnResize() override;
	void DoLayout() override;
};




} } // ork::ui