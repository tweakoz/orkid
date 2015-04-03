////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

namespace ork { namespace tool { 
void FindAssetChoices(const file::Path& sdir, CChoiceList* choice_list, const std::string& wildcard);
}}

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool { namespace ged {
///////////////////////////////////////////////////////////////////////////////
class UserFileChoices : public ork::tool::CChoiceList
{
	file::Path	mBaseDir;
	std::string	mWildCard;
public:

	virtual void EnumerateChoices( bool bforcenocache=false )
	{
		FindAssetChoices( mBaseDir, mWildCard );
	}

	UserFileChoices( const file::Path& basedir, std::string WildCard )
		: mBaseDir( basedir )
		, mWildCard( WildCard )
	{
	}
};
///////////////////////////////////////////////////////////////////////////////
template<typename IODriver> void GedFileNode<IODriver>::Describe() {}
///////////////////////////////////////////////////////////////////////////////
template <typename IODriver>
void GedFileNode<IODriver>::OnMouseDoubleClicked(const ork::ui::Event& ev)
{
	OnCreateObject();	
}
///////////////////////////////////////////////////////////////////////////////
template <typename IODriver>
void GedFileNode<IODriver>::DoDraw( lev2::GfxTarget* pTARG )
{
	int inamw = GetNameWidth()+8;
	//int ilabw = GetLabelWidth()+8;

	int ity = get_text_center_y();

	//GedItemNode::Draw( pTARG );
	const char* plab = mLabel.c_str();
	GetSkin()->DrawBgBox( this, miX+inamw, miY+2, miW-inamw, miH-4, GedSkin::ESTYLE_BACKGROUND_2 );
	GetSkin()->DrawText( this, miX+inamw+2, ity, plab );
	GetSkin()->DrawText( this, miX+6, ity, mName.c_str() );
}
///////////////////////////////////////////////////////////////////////////////
template < typename IODriver >
void GedFileNode<IODriver>::SetLabel()
{
	file::Path pth;

	mIoDriver.GetValue( pth );

	if( pth.length() == 0 )
	{
		mLabel = "none";
	}
	else
	{
		mLabel = pth.c_str();
	}
}
///////////////////////////////////////////////////////////////////////////////
template < typename IODriver >
GedFileNode<IODriver>::GedFileNode( ObjModel& mdl, const char* name, const reflect::IObjectProperty* prop, Object* obj )
	: GedItemNode( mdl, name, prop, obj )
	, mIoDriver( mdl, prop, obj )
{	
}
///////////////////////////////////////////////////////////////////////////////
template <typename IODriver>
void GedFileNode<IODriver>::OnCreateObject()
{
	ConstString anno = GetOrkProp()->GetAnnotation( "editor.filetype" );

	if( anno.length() )
	{
		std::string wildcard = CreateFormattedString( "*.%s", anno.c_str() );

		std::string urlbasetxt = "data://";

		ConstString anno_urlbase = GetOrkProp()->GetAnnotation( "editor.filebase" );

		if( anno_urlbase.length() )
		{
			urlbasetxt = anno_urlbase.c_str();
		}

		UserFileChoices chclist( urlbasetxt.c_str(), wildcard );
		chclist.EnumerateChoices();

		QMenu qm;

		QAction *pact = qm.addAction( "none" );
		pact->setData( QVariant( "none" ) );
		QAction *pact2 = qm.addAction( "reload" );
		pact2->setData( QVariant( "reload" ) );

		QMenu* qm2 = chclist.CreateMenu();
		qm.addMenu( qm2 );

		pact = qm.exec(QCursor::pos());

		if( pact )
		{
			QVariant UserData = pact->data();
			QString UserName = UserData.toString();
			std::string pname = UserName.toAscii().data();

			file::Path apath( pname.c_str() );

			apath.SetExtension( anno.c_str() );
			
			if( pname == "none" )
			{
				mModel.SigPreNewObject();
				mIoDriver.SetValue( file::Path("") );
				mModel.SigModelInvalidated();
			}
			else if( pname == "reload" )
			{
				mModel.SigPreNewObject();
				//file::Path gpath;
				//objprop->Get(gpath, GetOrkObj() );
				mModel.SigModelInvalidated();
			}
			else
			{
				mModel.SigPreNewObject();
				mIoDriver.SetValue( apath );
				mModel.SigModelInvalidated();
			}
			SetLabel();
				
		}
	}	
}
///////////////////////////////////////////////////////////////////////////////
}}} // namespace ork { namespace tool { namespace ged {
///////////////////////////////////////////////////////////////////////////////

