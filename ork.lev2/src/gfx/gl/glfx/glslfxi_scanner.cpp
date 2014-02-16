#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include "../gl.h"
#include "glslfxi.h"
#include "glslfxi_scanner.h"
#include <ork/file/file.h>
#include <regex>
#include <stdlib.h>


/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
/////////////////////////////////////////////////////////////////////////////////////////////////

GlslFxScanViewRegex::GlslFxScanViewRegex(const char* pr,bool inverse)
	: mRegex(pr)
	, mInverse(inverse)
{

}
bool GlslFxScanViewRegex::Test(const token& t)
{
	bool match = std::regex_match(t.text,mRegex);
	return match xor mInverse;
}

GlslFxScannerView::GlslFxScannerView( const GlslFxScanner& s, GlslFxScanViewFilter& f  )
	: mScanner(s)
	, mFilter(f)
	, mBlockTerminators( "(fxconfig|uniform_block|vertex_interface|tessctrl_interface|tesseval_interface|geometry_interface|fragment_interface|libblock|state_block|vertex_shader|tessctrl_shader|tesseval_shader|fragment_shader|geometry_shader|technique|pass)")
	, mStart(0)
	, mEnd(0)
	, mBlockType(0)
	, mBlockName(0)
	, mBlockOk(false)
{
}

void GlslFxScannerView::ScanRange( size_t ist, size_t ien )
{
	for( size_t i=ist; i<=ien; i++ )
	{
		const token& t = mScanner.tokens[i];
		bool bt = mFilter.Test(t);
		if( bt )
		{
			mIndices.push_back(i);
		}
	}
}

size_t GlslFxScannerView::GetBlockEnd() const
{
	return GetTokenIndex(mEnd);
}

std::string GlslFxScannerView::GetBlockName() const
{
	return GetToken(mBlockName)->text;	
}

void GlslFxScannerView::ScanBlock( size_t is )
{
	size_t max_t = mScanner.tokens.size();

	int ibracelev = 0;
	int istate = 0;

	const token& block_name = mScanner.tokens[is];
	mBlockName = is;

	for( size_t i=is; i<max_t; i++ )
	{	const token& t = mScanner.tokens[i];
		bool is_term = std::regex_match(t.text,mBlockTerminators);
		
		bool is_open = ( t.text == "{" );
		bool is_close = ( t.text == "}" );

		//printf( "itok<%d> t<%s> istate<%d> is_open<%d> is_close<%d> is_term<%d>\n",
		//		i, t.text.c_str(), istate, int(is_open), int(is_close), int(is_term) );

		switch( istate )
		{
			case 0:	// have not yet found block type
			{
				if( is_term )
				{	
					mBlockType = mIndices.size();
					mBlockName = mBlockType+1;
					istate = 1;
					mIndices.push_back(i);
				}
				else
				{
					mIndices.push_back(i);
					assert( false==is_open && false==is_close );
				}
				break;
			}
			case 1:	// have not yet found starting brace
			{	bool is_deco = ( t.text == ":" );
				if( is_open )
				{	assert(ibracelev==0);
					mStart = mIndices.size();
					ibracelev++;
					istate=2;
					mIndices.push_back(i);
				}
				else if( is_deco )
				{
					i++;
					mBlockDecorators.push_back(i);
				}
				else
				{	assert(false==is_close);
					if( is_term )
						assert(i>is);				
					if( mFilter.Test(t) )
						mIndices.push_back(i);
				}
				break;
			}
			case 2:	// content
				if( is_open )
				{	ibracelev++;
					mIndices.push_back(i);
				}
				else if( is_close )
				{	ibracelev--;
					mIndices.push_back(i);
					if( ibracelev==0 )
					{	mEnd = mIndices.size()-1;
						mBlockOk = true;
						return;
					}
				}
				else
				{	
					if( mFilter.Test(t) )
						mIndices.push_back(i);
				}
				break;

		}
	}
}

void GlslFxScannerView::Dump()
{
	printf( "GlslFxScannerView<%p>::Dump()\n",this);

	printf( " mBlockOk<%d>\n",int(mBlockOk));
	printf( " mStart<%d>\n",int(mStart));
	printf( " mEnd<%d>\n",int(mEnd));
	printf( " mBlockType<%d>\n",int(mBlockType));
	printf( " mBlockName<%d>\n",int(mBlockName));

	int i = 0;
	for( int tokidx : mIndices )
	{
		if( tokidx < mScanner.tokens.size() )
		{
			printf( "tok<%d> idx<%d> val<%s>\n",
				i, tokidx,
				mScanner.tokens[tokidx].text.c_str() );
		}
		i++;
	}
}

const token* GlslFxScanner::GetToken(size_t i) const
{
	const token* pt = nullptr;
	if( i < tokens.size() )
	{
		pt = & tokens[i];
	}
	return pt;
}

const token* GlslFxScannerView::GetToken(size_t i) const
{
	const token* pt = nullptr;
	if( i<mIndices.size() )
	{	int tokidx = mIndices[i];
		pt = mScanner.GetToken(tokidx);
	}

	return pt;
}

const token* GlslFxScannerView::GetBlockDecorator(size_t i) const
{
	const token* pt = nullptr;

	if( i<mBlockDecorators.size() )
	{
		int tokidx = mBlockDecorators[i];
		pt = mScanner.GetToken(tokidx);
	}
	return pt;
}

size_t GlslFxScannerView::GetTokenIndex(size_t i) const
{
	size_t ret = ~0;
	if( i<mIndices.size() )
	{
		ret = mIndices[i];
	}
	return ret;
}

GlslFxScanner::GlslFxScanner()
	: ss(ESTA_NONE)
	, cur_token("",0,0)
	, ifilelen(0)
{

}
void GlslFxScanner::FlushToken()
{
	if( cur_token.text.length() )
		tokens.push_back( cur_token );
	cur_token.text="";
	cur_token.iline=0;
	cur_token.icol=0;
	ss=ESTA_NONE;
}
/////////////////////////////////////////
void GlslFxScanner::AddToken( const token& tok )
{
	tokens.push_back(tok);
	cur_token.text="";
	cur_token.iline=0;
	cur_token.icol=0;
	ss=ESTA_NONE;
}
/////////////////////////////////////////
void GlslFxScanner::Scan()
{
	
	int iscanst_cpp_comment = 0;
	int iscanst_c_comment = 0;
	int iscanst_whitespace = 0;
	int iscanst_dqstring = 0;
	int iscanst_sqstring = 0;
	
	int iline = 0;
	int icol = 0;
	
	int itoksta_line = 0;
	int itoksta_colm = 0;
	
	bool b_in_number = false;
	
	for( size_t i=0; i<ifilelen; i++ )
	{
		char PCH = (i==0) ? 0 : fxbuffer[i-1];
		char CH = fxbuffer[i];
		char NCH = (i<ifilelen-1) ? fxbuffer[i+1] : 0;
		
		char ch_buf[2];
		ch_buf[0] = CH;
		ch_buf[1] = 0;
		
		bool benctok = false;
		
		int adv_col = 1;
		int adv_lin = 0;
		
		switch( ss )
		{
			case ESTA_NONE:
				if( (CH=='/') && (NCH=='/') ) { ss=ESTA_CPP_COMMENT; iscanst_cpp_comment++; i++; }
				else if( (CH=='/') && (NCH=='*') ) { ss=ESTA_C_COMMENT; iscanst_c_comment++; i++; }
				else if( CH=='\'' ) { ss=ESTA_SQ_STRING; benctok=true; }
				else if( CH=='\"' ) { ss=ESTA_DQ_STRING; benctok=true; }
				else if( is_spc(CH) ) { ss=ESTA_WSPACE; }
				else if( CH=='\n' )
				{
					AddToken( token( "\n", iline, icol ) );
					adv_lin = 1;
				}
				else if( is_septok(CH) )
				{
					if(		((CH=='=')&&(NCH=='=')) || ((CH=='!')&&(NCH=='='))
					   ||	((CH=='*')&&(NCH=='=')) || ((CH=='/')&&(NCH=='='))
					   ||	((CH=='&')&&(NCH=='=')) || ((CH=='|')&&(NCH=='='))
					   ||	((CH=='&')&&(NCH=='&')) || ((CH=='|')&&(NCH=='|'))
					   ||	((CH=='<')&&(NCH=='<')) || ((CH=='>')&&(NCH=='>'))
					   ||	((CH=='<')&&(NCH=='=')) || ((CH=='>')&&(NCH=='='))
					   ||	((CH==':')&&(NCH==':')) || ((CH=='$')&&(NCH=='('))
					   ||	((CH=='+')&&(NCH=='+')) || ((CH=='-')&&(NCH=='-'))
					   ||	((CH=='+')&&(NCH=='=')) || ((CH=='-')&&(NCH=='='))
					   )
					{
						char ch_buf2[3];
						ch_buf2[0] = CH;
						ch_buf2[1] = NCH;
						ch_buf2[2] = 0;
						AddToken( token( ch_buf2, iline, icol ) );
						i++;
					}
					else
						AddToken( token( ch_buf, iline, icol ) );
					
				}
				else
				{	ss = ESTA_CONTENT;
					benctok=true;
					cur_token.iline = iline;
					cur_token.icol = icol;
				}
				break;
			case ESTA_C_COMMENT:
				if( (CH=='/') && (PCH=='*') ) { iscanst_c_comment--; if( iscanst_c_comment==0 ) ss=ESTA_NONE; }
				break;
			case ESTA_CPP_COMMENT:
				if( CH=='/' ) {}
				if( CH=='\n' ) { ss=ESTA_NONE; adv_lin=1; }
				break;
			case ESTA_DQ_STRING:
				if( CH=='\"' ) { cur_token.text += ch_buf; FlushToken(); }
				else { benctok=true; }
				break;
			case ESTA_SQ_STRING:
				if( CH=='\'' ) { cur_token.text += ch_buf; FlushToken(); }
				else { benctok=true; }
				break;
			case ESTA_WSPACE:
				if( (false == is_spc(CH))&&(CH!='\n') ) { ss=ESTA_NONE; i--; }
				break;
			case ESTA_CONTENT:
			{
				if( is_septok(CH) )
				{
					FlushToken();
					i--;
				}
				else if( CH=='\n' )
				{
					FlushToken();
					adv_lin = 1;
				}
				else if( is_spc(CH) )
				{
					FlushToken();
				}
				else if( is_content(CH) )
				{
					benctok = true;
				}
				break;
			}
		}
		if( benctok )
		{
			cur_token.text += ch_buf;
		}
		if( adv_col )
		{
			icol+=adv_col;
		}
		if( adv_lin )
		{
			iline++;
			icol=0;
		}
		
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////
}} //namespace ork { namespace lev2 {
/////////////////////////////////////////////////////////////////////////////////////////////////
