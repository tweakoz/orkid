////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// GlslFx Scanner/Parser
//  this replaces CgFx for OpenGL 3.x and OpenGL ES 2.x
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include "../gl.h"
#if defined(_USE_GLSLFX)
#include "glslfxi.h"
#include "glslfxi_scanner.h"
#include <ork/file/file.h>
#include <regex>
#include <stdlib.h>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
/////////////////////////////////////////////////////////////////////////////////////////////////

struct GlSlFxParser
{
	int itokidx;
	const GlslFxScanner& scanner;
	GlslFxContainer* mpContainer;
	
	///////////////////////////////////////////////////////////
	GlSlFxParser( const GlslFxScanner& s ) : scanner(s), mpContainer(nullptr) {}
	///////////////////////////////////////////////////////////
	bool IsTokenOneOfTheBlockTypes( const token& tok )
	{
		std::regex regex_block( "(fxconfig|vertex_interface|fragment_interface|libblock|state_block|vertex_shader|fragment_shader|technique|pass)");
		return std::regex_match(tok.text, regex_block);
	}
	///////////////////////////////////////////////////////////
	int FindEndOfBlock( const int block_name, int block_start )
	{
		const std::vector<token>& tokens = scanner.tokens;
		
		const token& block_name_tok = tokens[block_name];
		const token& block_beg_tok = tokens[block_start];
		printf( "FindEndOfBlock BlockName<%s> BraceTok<%s>\n", block_name_tok.text.c_str(), block_beg_tok.text.c_str() );
		
		if( block_beg_tok.text=="\n" )
			block_start++;

		int itok = block_start+1;
		OrkAssert( block_beg_tok.text == "{" );
		int ibracelev = 1;
		
		int iend = 0;
		
		while( ibracelev>0 )
		{
			const token& tok = tokens[itok];
			if( tok.text == "{" )
				ibracelev++;
			else if ( tok.text == "}" )
			{	ibracelev--;
				if( ibracelev==0 )
					iend = itok;
			}
			//else if( IsTokenOneOfTheBlockTypes( tok ) )
			//{
			//	printf( "ERROR: expected }, got <%s> instead\n", tok.text.c_str() );
			//	OrkAssert(false);
			//}
			itok++;
		}
		
		return iend;
	}
	///////////////////////////////////////////////////////////
	GlslFxConfig* ParseFxConfig()
	{
		GlslFxScanViewRegex r("(\n)",true);
		GlslFxScannerView v( scanner, r );
		v.ScanBlock(itokidx);

		int iend = FindEndOfBlock( itokidx+1, itokidx+2 );
		const token& etok = scanner.tokens[iend+1];
		printf( "ParseFxConfig Eob<%d> ScEnd<%d> Next<%s>\n", iend, v.mEnd, etok.text.c_str() );
		v.Dump();

		GlslFxConfig* pcfg = new GlslFxConfig;
		pcfg->mName = scanner.tokens[ itokidx+1 ].text;
		itokidx = iend+1;
		return pcfg;
	}
	///////////////////////////////////////////////////////////
	GlslFxStreamInterface* ParseFxInterface()
	{	
		GlslFxScanViewRegex r("(\n)",true);
		GlslFxScannerView v( scanner, r );
		v.ScanBlock(itokidx);
		v.Dump();

		int iend = FindEndOfBlock( itokidx+1, itokidx+2 );
		const token& etok = scanner.tokens[iend+1];
		const auto& toks = scanner.tokens;

		//printf( "ParseFxInterface Eob<%d> Next<%s>\n", iend, etok.text.c_str() );
		GlslFxStreamInterface* psi = new GlslFxStreamInterface;
		psi->mName = toks[ itokidx+1 ].text;
		////////////////////////
		OrkAssert( toks[itokidx+2].text=="{" );
		for( int i=itokidx+3; i<iend; )
		{
			const token& vt_tok = toks[i];
			const token& dt_tok = toks[i+1];
			const token& nam_tok = toks[i+2];
			
			//printf( "  ParseFxInterface Tok<%s>\n", vt_tok.text.c_str() );
			
			if( vt_tok.text == "uniform" )
			{
				GlslFxUniform* puni = mpContainer->MergeUniform( nam_tok.text );
				puni->mTypeName = dt_tok.text;
				psi->mUniforms[ nam_tok.text ] = puni;
				if( toks[i+3].text=="[" )
				{
					assert(toks[i+5].text=="]");
					puni->mArraySize = atoi(toks[i+4].text.c_str());
					printf( "uniname<%s> arraysize<%d>\n", nam_tok.text.c_str(), puni->mArraySize );
					i += 7;
					//assert(false);
				}
				else
				{
					i += 4;
				}
			}
			else if( vt_tok.text == "in" )
			{
				int iloc = int(psi->mAttributes.size());
				GlslFxAttribute* pattr = new GlslFxAttribute( nam_tok.text );
				pattr->mTypeName = dt_tok.text;
				pattr->mDirection = "in";
				pattr->mLocation = iloc;
				psi->mAttributes[ nam_tok.text ] = pattr;
				if( toks[i+3].text==":" )
				{
					pattr->mSemantic = toks[i+4].text;
					printf( "SEMANTIC<%s>\n", pattr->mSemantic.c_str() );	
					i += 6;
				}
				else
				{
					assert( toks[i+3].text==";" );
					i += 4;
				}
			}
			else if( vt_tok.text == "out" )
			{
				int iloc = int(psi->mAttributes.size());
				GlslFxAttribute* pattr = new GlslFxAttribute( nam_tok.text );
				pattr->mTypeName = dt_tok.text;
				pattr->mDirection = "out";
				pattr->mLocation = iloc;
				psi->mAttributes[ nam_tok.text ] = pattr;
				i += 4;
			}
			else if( vt_tok.text == "\n" )
			{
				i++;
			}
			else
			{
				printf( "invalid token<%s>\n", vt_tok.text.c_str() );
				OrkAssert(false);
			}
		}
		////////////////////////
		itokidx = iend+1;
		return psi;
	}
	///////////////////////////////////////////////////////////
	GlslFxStateBlock* ParseFxStateBlock()
	{
		GlslFxScanViewRegex r("(\n)",true);
		GlslFxScannerView v( scanner, r );
		v.ScanBlock(itokidx);
		v.Dump();

		int iend = FindEndOfBlock( itokidx+1, itokidx+2 );
		const token& etok = scanner.tokens[iend+1];
		//printf( "ParseFxStateBlock Eob<%d> Next<%s>\n", iend, etok.text.c_str() );
		GlslFxStateBlock* psb = new GlslFxStateBlock;
		psb->mName = scanner.tokens[ itokidx+1 ].text;
		mpContainer->AddStateBlock( psb );
		//////////////////////

		auto& apptors = psb->mApplicators;

		//////////////////////
		for( int i=itokidx+3; i<iend; )
		{
			const token& vt_tok = scanner.tokens[i];
			//printf( "  ParseFxStateBlock Tok<%s>\n", vt_tok.text.c_str() );
			if( vt_tok.text == "inherits" )
			{
				const token& parent_tok = scanner.tokens[i+1];
				GlslFxStateBlock* ppar = mpContainer->GetStateBlock( parent_tok.text );
				OrkAssert(ppar!=nullptr);
				psb->mApplicators = ppar->mApplicators;
				i += 3;
			}
			else if( vt_tok.text == "CullTest" )
			{
				const std::string& mode = scanner.tokens[i+2].text;
				if( mode=="OFF" )
					psb->AddStateFn( [=](GfxTarget*t)
					{	t->RSI()->SetCullTest(lev2::ECULLTEST_OFF);
					} );
				else if( mode=="PASS_FRONT" )
					psb->AddStateFn( [=](GfxTarget*t)
					{	t->RSI()->SetCullTest(lev2::ECULLTEST_PASS_FRONT);
					} );
				else if( mode=="PASS_BACK" )
					psb->AddStateFn( [=](GfxTarget*t)
					{	t->RSI()->SetCullTest(lev2::ECULLTEST_PASS_BACK);
					} );

				i += 4;
			}
			else if( vt_tok.text == "DepthMask" )
			{
				const std::string& mode = scanner.tokens[i+2].text;
				bool bena = (mode == "true");
				psb->AddStateFn( [=](GfxTarget*t)
				{	t->RSI()->SetZWriteMask(bena);
				} );
				printf( "DepthMask<%d>\n", int(bena) );
				i += 4;
			}
			else if( vt_tok.text == "DepthTest" )
			{
				const std::string& mode = scanner.tokens[i+2].text;
				if( mode=="OFF" )
					psb->AddStateFn( [=](GfxTarget*t)
					{	t->RSI()->SetDepthTest(lev2::EDEPTHTEST_OFF);
					} );
				else if( mode=="LESS" )
					psb->AddStateFn( [=](GfxTarget*t)
					{	t->RSI()->SetDepthTest(lev2::EDEPTHTEST_LESS);
					} );
				else if( mode=="LEQUALS" )
					psb->AddStateFn( [=](GfxTarget*t)
					{	t->RSI()->SetDepthTest(lev2::EDEPTHTEST_LEQUALS);
					} );
				else if( mode=="GREATER" )
					psb->AddStateFn( [=](GfxTarget*t)
					{	t->RSI()->SetDepthTest(lev2::EDEPTHTEST_GREATER);
					} );
				else if( mode=="GEQUALS" )
					psb->AddStateFn( [=](GfxTarget*t)
					{	t->RSI()->SetDepthTest(lev2::EDEPTHTEST_GEQUALS);
					} );
				else if( mode=="EQUALS" )
					psb->AddStateFn( [=](GfxTarget*t)
					{	t->RSI()->SetDepthTest(lev2::EDEPTHTEST_EQUALS);
					} );
				i += 4;
			}
			else if( vt_tok.text == "BlendMode" )
			{
				const std::string& mode = scanner.tokens[i+2].text;
				if( mode=="ADDITIVE" )
					psb->AddStateFn( [=](GfxTarget*t)
					{	t->RSI()->SetBlending(lev2::EBLENDING_ADDITIVE);
					} );
				else if( mode == "ALPHA_ADDITIVE" )
					psb->AddStateFn( [=](GfxTarget*t)
					{	t->RSI()->SetBlending(lev2::EBLENDING_ALPHA_ADDITIVE);
					} );
				else if( mode == "ALPHA" )
					psb->AddStateFn( [=](GfxTarget*t)
					{	t->RSI()->SetBlending(lev2::EBLENDING_ALPHA);
					} );
				i += 4;
			}
			else if( vt_tok.text == "\n" )
			{
				i++;
			}
			else
			{
				OrkAssert(false);
			}
		}
		//////////////////////
		itokidx = iend+1;
		return psb;
	}
	///////////////////////////////////////////////////////////
	GlslFxLibBlock* ParseLibraryBlock()
	{
		GlslFxScanViewRegex r("(\n)",true);
		GlslFxScannerView v( scanner, r );
		v.ScanBlock(itokidx);
		v.Dump();

		auto pret = new GlslFxLibBlock;
		pret->mName = scanner.tokens[ itokidx+1 ].text;

		int iend = FindEndOfBlock( itokidx+1, itokidx+2 );
		const token& etok = scanner.tokens[iend+1];
		int indent = 0;
		std::string libblktext;
		for( int i=itokidx+3; i<iend; i++ )
		{
			for( int in=0; in<indent; in++ )
				libblktext += " ";

			const std::string& cur_tok = scanner.tokens[i].text;
			//printf( "  ParseFxShaderCommon Tok<%s>\n", vt_tok.text.c_str() );
			libblktext += cur_tok + " ";
			if( cur_tok==";" )
			{	
			}
			else if( cur_tok=="{" )
				indent++;
			else if( cur_tok=="}" )
			{
				indent--;
				if( indent == 0 )
					libblktext += "\n";
			}
		}

		pret->mLibBlockText = libblktext;

		printf( "libblktext\n///////////////////////\n%s\n/////////////////////\n", libblktext.c_str() );

		itokidx = iend+1;

		return pret;
	}
	///////////////////////////////////////////////////////////
	int ParseFxShaderCommon(GlslFxShader* pshader)
	{
		GlslFxScanViewRegex r("()",true);
		GlslFxScannerView v( scanner, r );
		v.ScanBlock(itokidx);
		v.Dump();

		pshader->mpContainer = mpContainer;

		///////////////////////////////////
		std::string shadername = scanner.tokens[ itokidx+1 ].text;
		std::string shadercln = scanner.tokens[ itokidx+2 ].text;
		std::string outiface = scanner.tokens[ itokidx+3 ].text;

		GlslFxLibBlock* plibblock = nullptr;

		int iblockstart = itokidx+4;

		//////////////////////////////////////////////
		// enumerate lib blocks
		//////////////////////////////////////////////

		std::vector<GlslFxLibBlock*> lib_blocks;

		std::string t4 = scanner.tokens[ iblockstart ].text;
		while( t4 == ":" )
		{
			const std::string& libt = scanner.tokens[ iblockstart+1 ].text;
			auto it = mpContainer->mLibBlocks.find(libt);
			if( it != mpContainer->mLibBlocks.end() )
			{
				auto plibblock = it->second;
				lib_blocks.push_back( plibblock );
			}
			iblockstart += 2;
			t4 = scanner.tokens[ iblockstart ].text;
		}

		//////////////////////////////////////////////

		bool bvtx = pshader->mShaderType == GL_VERTEX_SHADER;
		

		int iend = FindEndOfBlock( itokidx+1, iblockstart );
		const token& etok = scanner.tokens[iend+1];
		//printf( "ParseFxShaderCommon Eob<%d> Next<%s>\n", iend, etok.text.c_str() );
		///////////////////////////////////
		GlslFxStreamInterface* iface = bvtx ? mpContainer->GetVertexInterface( outiface ) : mpContainer->GetFragmentInterface( outiface );
		OrkAssert( shadercln == ":" );
		OrkAssert( iface != nullptr );
		pshader->mpInterface = iface;
		///////////////////////////////////
#if defined(USE_GL3)
		std::string shaderbody = "#version 150 core\n";
#else
		std::string shaderbody = "#version 120\n";
#endif
		for( GlslFxStreamInterface::UniMap::const_iterator
			itu=iface->mUniforms.begin();
			itu!=iface->mUniforms.end();
			itu++ )
		{
			GlslFxUniform* pu = itu->second;
			shaderbody += "uniform ";
			shaderbody += pu->mTypeName + " ";
			shaderbody += pu->mName;

			if( pu->mArraySize )
			{
				ork::FixedString<32> fxs;
				fxs.format("[%d]", pu->mArraySize );
				shaderbody += std::string(fxs.c_str());
			}

			shaderbody += ";\n";
		}
		for( GlslFxStreamInterface::AttrMap::const_iterator
			ita=iface->mAttributes.begin();
			ita!=iface->mAttributes.end();
			ita++ )
		{
			GlslFxAttribute* pa = ita->second;
#if defined(USE_GL3)
			shaderbody += pa->mDirection + " ";
#endif
			shaderbody += pa->mTypeName + " ";
			shaderbody += pa->mName + ";\n";
		}
		///////////////////////////////////
		// inject libblock code
		///////////////////////////////////
		for( const auto& libblk : lib_blocks )
		{
			shaderbody += "// libblock<" + libblk->mName + "> ///////////////////////////////////\n";
			shaderbody += libblk->mLibBlockText;
			shaderbody += "///////////////////////////////////////////////////////////////////\n";
		}	
		///////////////////////////////////
		shaderbody += "void " + shadername + "()\n{";
		bool bnewline = true;
		int indent = 1;
		for( int i=iblockstart+1; i<iend; i++ )
		{
			if( bnewline )
			{
				for( int in=0; in<indent; in++ )
					shaderbody += "\t";
			}
			bnewline=false;
			const std::string& cur_tok = scanner.tokens[i].text;
			//printf( "  ParseFxShaderCommon Tok<%s>\n", vt_tok.text.c_str() );
			shaderbody += cur_tok + " ";
			if( cur_tok==";" )
			{	shaderbody += "\n";
				bnewline = true;
			}
			else if( cur_tok=="{" )
				indent++;
			else if( cur_tok=="}" )
				indent--;
		}
		shaderbody += "}\n";
		///////////////////////////////////
		printf( "shaderbody\n" );
		printf( "///////////////////////////////\n" );
		printf( "%s", shaderbody.c_str() );
		printf( "///////////////////////////////\n" );
		///////////////////////////////////
		pshader->mName = shadername;
		pshader->mShaderText = shaderbody;
		///////////////////////////////////
		return iend+1;
	}
	///////////////////////////////////////////////////////////
	GlslFxShaderVtx* ParseFxVertexShader()
	{
		GlslFxShaderVtx* pshader = new GlslFxShaderVtx();
		itokidx = ParseFxShaderCommon( pshader );
		return pshader;
	}
	///////////////////////////////////////////////////////////
	GlslFxShaderFrg* ParseFxFragmentShader()
	{
		GlslFxShaderFrg* pshader = new GlslFxShaderFrg();
		itokidx = ParseFxShaderCommon( pshader );
		return pshader;
	}
	///////////////////////////////////////////////////////////
	GlslFxTechnique* ParseFxTechnique()
	{
		int iend = FindEndOfBlock( itokidx+1, itokidx+2 );
		const token& etok = scanner.tokens[iend+1];
		//printf( "ParseFxTechnique Eob<%d> Next<%s>\n", iend, etok.text.c_str() );
		GlslFxTechnique* ptek = new GlslFxTechnique( scanner.tokens[ itokidx+1 ].text );
		////////////////////////////////////////
		OrkAssert( scanner.tokens[itokidx+2].text == "{" );
		////////////////////////////////////////
		for( int i=itokidx+3; i<iend; )
		{
			const token& vt_tok = scanner.tokens[i];
			//printf( "  ParseFxTechnique Tok<%s>\n", vt_tok.text.c_str() );
			if( vt_tok.text == "fxconfig" )
			{
				i+=4;
			}
			else if( vt_tok.text == "pass" )
			{
				i = ParseFxPass(i,ptek);
			}
			else if( vt_tok.text == "\n" )
			{
				i++;
			}
			else
			{
				OrkAssert(false);
			}
		}
		////////////////////////////////////////
		itokidx = iend+1;
		return ptek;
	}
	///////////////////////////////////////////////////////////
	int ParseFxPass( int istart, GlslFxTechnique* ptek )
	{
		int iend = FindEndOfBlock( istart+1, istart+2 );
		const token& etok = scanner.tokens[iend+1];
		std::string name = scanner.tokens[ istart+1 ].text;
		//printf( "ParseFxPass Name<%s> Eob<%d> Next<%s>\n", name.c_str(), iend, etok.text.c_str() );
		GlslFxPass* ppass = new GlslFxPass( name );
		////////////////////////////////////////
		OrkAssert( scanner.tokens[istart+2].text == "{" );
		/////////////////////////////////////////////
		for( int i=istart+3; i<iend; )
		{
			const token& vt_tok = scanner.tokens[i];
			//printf( "  ParseFxPass Tok<%s>\n", vt_tok.text.c_str() );
			
			if( vt_tok.text == "vertex_shader" )
			{
				std::string vsnam = scanner.tokens[i+2].text;
				GlslFxShader* pshader = mpContainer->GetVertexProgram( vsnam );
				OrkAssert( pshader != nullptr );
				ppass->mVertexProgram = pshader;
				i+=4;
			}
			else if( vt_tok.text == "fragment_shader" )
			{
				std::string fsnam = scanner.tokens[i+2].text;
				GlslFxShader* pshader = mpContainer->GetFragmentProgram( fsnam );
				OrkAssert( pshader != nullptr );
				ppass->mFragmentProgram = pshader;
				i+=4;
			}
			else if( vt_tok.text == "state_block" )
			{
				std::string sbnam = scanner.tokens[i+2].text;
				GlslFxStateBlock* psb = mpContainer->GetStateBlock( sbnam );
				OrkAssert(psb!=nullptr);
				ppass->mStateBlock = psb;
				i+=4;
			}
			else if( vt_tok.text == "\n" )
			{
				i++;
			}
			else
			{
				OrkAssert(false);
			}
		}
		/////////////////////////////////////////////
		/////////////////////////////////////////////
		ptek->AddPass( ppass );
		/////////////////////////////////////////////
		return iend+1;
	}
	///////////////////////////////////////////////////////////
	GlslFxContainer* Parse( const std::string& fxname)
	{
		const std::vector<token>& tokens = scanner.tokens;
		
		//printf( "NumTokens<%d>\n", int(tokens.size()) );
		itokidx = 0;
		while( itokidx<tokens.size() )
		{
			const token& tok = tokens[itokidx++];
			//printf( "tok<%d> <%s>\n", itokidx, tok.text.c_str() );
		}
		itokidx = 0;
		
		mpContainer = new GlslFxContainer( fxname.c_str() );
		bool bOK = true;
		
		while( itokidx<tokens.size() )
		{
			const token& tok = tokens[itokidx];
			printf( "token<%d> iline<%d> col<%d> text<%s>\n", itokidx, tok.iline+1, tok.icol+1, tok.text.c_str() );
			
			if( tok.text == "\n" )
			{
				itokidx++;
			}
			else if( tok.text == "fxconfig" )
			{
				GlslFxConfig* pconfig = ParseFxConfig();
				mpContainer->AddConfig( pconfig );
			}
			else if( tok.text == "libblock" )
			{
				auto lb = ParseLibraryBlock();
				mpContainer->AddLibBlock(lb);
			}
			else if( tok.text == "vertex_interface" )
			{
				GlslFxStreamInterface* pif = ParseFxInterface();
				mpContainer->AddVertexInterface( pif );
			}
			else if( tok.text == "fragment_interface" )
			{
				GlslFxStreamInterface* pif = ParseFxInterface();
				mpContainer->AddFragmentInterface( pif );
			}
			else if( tok.text == "state_block" )
			{
				GlslFxStateBlock* psblock = ParseFxStateBlock();
				mpContainer->AddStateBlock( psblock );
			}
			else if( tok.text == "vertex_shader" )
			{
				GlslFxShaderVtx* pshader = ParseFxVertexShader();
				mpContainer->AddVertexProgram( pshader );
			}
			else if( tok.text == "fragment_shader" )
			{
				GlslFxShaderFrg* pshader = ParseFxFragmentShader();
				mpContainer->AddFragmentProgram( pshader );
			}
			else if( tok.text == "technique" )
			{
				GlslFxTechnique* ptek = ParseFxTechnique();
				mpContainer->AddTechnique( ptek );
			}
			else
			{
				printf( "Unknown Token<%s>\n", tok.text.c_str() );
				OrkAssert(false);
			}
		}
		if( false==bOK )
		{
			delete mpContainer;
			mpContainer = nullptr;
		}
		return mpContainer;
	}
};



/*std::string::const_iterator start = fx_string.begin();
 std::string::const_iterator end = fx_string.end();
 boost::match_results<std::string::const_iterator> what;
 boost::match_flag_type flags = boost::match_default;
 
 while( boost::regex_search( start, end, what, re_container, flags ) )
 {
 // what[0] contains the whole string 
 // what[5] contains the class name. 
 // what[6] contains the template specialisation if any. 
 // add class name and position to map: 
 //m[std::string(what[5].first, what[5].second)
 //  + std::string(what[6].first, what[6].second)]
 //= what[5].first - file.begin();
 
 const char* match_start = fx_string.c_str()+(what[1].first-fx_string.begin());
 const char* match_end = fx_string.c_str()+(what[1].second-fx_string.begin());
 
 printf( "what5<%s> end<%s>\n", what[1].str().c_str(), match_end );
 // update search position:
 start = what[0].second;
 // update flags: 
 flags |= boost::match_prev_avail;
 flags |= boost::match_not_bob;
 
 }
 //	printf( "nummatch<%d>\n", int(res.size()) );
 
 //	boost::sregex_iterator it(fx_string.begin(), fx_string.end(), re_identifier );
 //  boost::sregex_iterator end;
 
 //for (; it != end; ++it)
 //{
 //  printf( "match<%s>\n", it->str().c_str() );
 // v.push_back(it->str()); or something similar     
 //}*/


//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

GlslFxContainer* LoadFxFromFile( const AssetPath& pth )
{
	GlslFxScanner scanner;
	GlSlFxParser parser(scanner);
	///////////////////////////////////
	CFile fx_file( pth, EFM_READ );
	OrkAssert( fx_file.IsOpen() );
	EFileErrCode eFileErr = fx_file.GetLength( scanner.ifilelen );
	OrkAssert( scanner.ifilelen<scanner.kmaxfxblen );
	eFileErr = fx_file.Read( scanner.fxbuffer, scanner.ifilelen );
	scanner.fxbuffer[scanner.ifilelen] = 0;
	///////////////////////////////////
	scanner.Scan();
	GlslFxContainer* pcont = parser.Parse( pth.c_str() );
	return pcont;
}

	
/////////////////////////////////////////////////////////////////////////////////////////////////
}}
/////////////////////////////////////////////////////////////////////////////////////////////////

#endif
