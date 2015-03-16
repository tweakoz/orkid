#pragma once

#include <ork/lev2/ui/widget.h>

namespace ork { namespace ui {

////////////////////////////////////////////////////////////////////

struct Panel : public Group
{
	Panel( const std::string & name, int x, int y, int w, int h );
	~Panel();

	void SetChild( Widget* pch);

	void Snap();

private:

	HandlerResult DoOnUiEvent( const Event &Ev ) override;
	void DoDraw(ui::DrawEvent& drwev) override;
	void DoLayout( void ) override;
	void DoOnEnter() override;
	void DoOnExit() override;
	HandlerResult DoRouteUiEvent( const Event& Ev ) override;

	Widget* mChild;
	int mPanelUiState;
	bool mDockedAtTop;
	int mCloseX, mCloseY;
};

}} // namespace ork { namespace ui {
