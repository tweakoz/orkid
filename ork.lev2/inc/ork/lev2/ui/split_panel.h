#pragma once

#include <ork/lev2/ui/widget.h>

namespace ork { namespace ui {

////////////////////////////////////////////////////////////////////

struct SplitPanel : public Group
{
	SplitPanel( const std::string & name, int x, int y, int w, int h );
	~SplitPanel();

	void SetChild1( Widget* pch);
	void SetChild2( Widget* pch);

	void EnableCloseButton() { mEnableCloseButton=true; }

private:

	HandlerResult DoOnUiEvent( const Event &Ev ) override;
	void DoDraw(ui::DrawEvent& drwev) override;
	void DoLayout( void ) override;
	void DoOnEnter() override;
	void DoOnExit() override;
	HandlerResult DoRouteUiEvent( const Event& Ev ) override;

	Widget* mChild1;
	Widget* mChild2;
	int mPanelUiState;
	bool mDockedAtTop;
	float mSplitVal;
	int mCloseX, mCloseY;
	bool mEnableCloseButton;
};

}} // namespace ork { namespace ui {
