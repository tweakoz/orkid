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

	HandlerResult DoOnUiEvent( event_constptr_t Ev ) override;
	void DoDraw(ui::drawevent_constptr_t drwev) override;
	void DoLayout( void ) override;
	void DoOnEnter() override;
	void DoOnExit() override;
	HandlerResult DoRouteUiEvent( event_constptr_t Ev ) override;

	Widget* mChild;
	int mPanelUiState;
	bool mDockedAtTop;
	int mCloseX, mCloseY;
};

}} // namespace ork { namespace ui {
