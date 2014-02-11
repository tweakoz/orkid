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

static const std::map<std::string,int> gattrsorter = 
{
	{"POSITION",0},
	{"NORMAL",1},
	{"COLOR0",2},
	{"COLOR1",3},
	{"TEXCOORD0",4},
	{"TEXCOORD0",5},
	{"TEXCOORD1",6},
	{"TEXCOORD2",7},
	{"TEXCOORD3",8},
	{"BONEINDICES",9},
	{"BONEWEIGHTS",10},
};

void GlslFxStreamInterface::Inherit(const GlslFxStreamInterface& par)
{
	bool is_vtx = mInterfaceType==GL_VERTEX_SHADER;
	bool is_geo = mInterfaceType==GL_GEOMETRY_SHADER;
	bool is_frg = mInterfaceType==GL_FRAGMENT_SHADER;

	bool par_is_vtx = par.mInterfaceType==GL_VERTEX_SHADER;

	bool types_match = mInterfaceType==par.mInterfaceType;
	bool geoinhvtx = is_geo&&par_is_vtx;
	bool inherit_ok = types_match|geoinhvtx;

	assert( inherit_ok );

	////////////////////////////////

	mPreamble = par.mPreamble;

	////////////////////////////////
	// convert vertex out attrs
	// to geom in array attrs
	////////////////////////////////

	bool conv_vtx_to_geo = ( is_geo && par_is_vtx );

	////////////////////////////////

	if( false == conv_vtx_to_geo )
		for( const auto& u : par.mUniforms )
	{
		auto it = mUniforms.find(u.first);
		assert(it==mUniforms.end()); // make sure there are no duplicate unifs
		
		mUniforms[u.first] = u.second;
	}
	
	for( const auto& a : par.mAttributes )
	{
		auto it = mAttributes.find(a.first);
		assert(it==mAttributes.end()); // make sure there are no duplicate attrs

		const GlslFxAttribute* src = a.second;

		if( conv_vtx_to_geo )
		{
			if( src->mDirection=="out" )
			{
				GlslFxAttribute* cpy = new GlslFxAttribute(src->mName,src->mSemantic);
				cpy->mTypeName = src->mTypeName;
				cpy->mDirection = "in";
				cpy->meType = src->meType;
				cpy->mArraySize = 1;
				cpy->mLocation = int(mAttributes.size());
				cpy->mComment = "// vtx->geo";

				mAttributes[a.first] = cpy;

				printf( "copied vtx->geo nam<%s> typ<%s>\n", src->mName.c_str(), src->mTypeName.c_str() );
			}
		}
		else
		{
			GlslFxAttribute* cpy = new GlslFxAttribute(src->mName,src->mSemantic);
			cpy->mTypeName = src->mTypeName;
			cpy->mDirection = src->mDirection;
			cpy->meType = src->meType;

			cpy->mLocation = int(mAttributes.size());
			mAttributes[a.first] = cpy;
		}
	}
}

struct GlSlFxParser
{
	int itokidx;
	GlslFxScanner& scanner;
	const AssetPath mPath;
	GlslFxContainer* mpContainer;
	
	///////////////////////////////////////////////////////////
	GlSlFxParser( const AssetPath& pth, GlslFxScanner& s ) 
		: mPath(pth)
		, scanner(s)
		, mpContainer(nullptr)
	{}
	///////////////////////////////////////////////////////////
	bool IsTokenOneOfTheBlockTypes( const token& tok )
	{
		std::regex regex_block( "(fxconfig|vertex_interface|fragment_interface|geometry_interface|libblock|state_block|vertex_shader|fragment_shader|geometry_shader|technique|pass)");
		return std::regex_match(tok.text, regex_block);
	}
	///////////////////////////////////////////////////////////
	GlslFxConfig* ParseFxConfig()
	{
		GlslFxScanViewRegex r("(\n)",true);
		GlslFxScannerView v( scanner, r );
		v.ScanBlock(itokidx);

		GlslFxConfig* pcfg = new GlslFxConfig;
		pcfg->mName = v.GetBlockName();

		int ist = v.mStart+1;
		int ien = v.mEnd-1;

		std::vector<std::string> imports;
		for( size_t i=ist; i<=ien; )
		{
			const token* vt_tok = v.GetToken(i);

			printf( "vt_tok<%s>\n", vt_tok->text.c_str() );

			if( vt_tok->text == "import" )
			{
				const token* impnam = v.GetToken(i+1);
				std::string p = impnam->text.substr(1,impnam->text.length()-2);
				imports.push_back(p);

				i +=3;
			}
			else 
				i++;
		}

		itokidx = v.GetBlockEnd()+1;

		for( const auto& imp : imports)
		{
			GlslFxScanner scanner2;

			file::Path::NameType a, b;
			mPath.Split(a,b,':');

			ork::FixedString<256> fxs;
			fxs.format("%s://%s", a.c_str(),imp.c_str());
			file::Path imppath = fxs.c_str();
			//assert(false);

			printf( "impnam<%s> a<%s> b<%s> imppath<%s>\n", imp.c_str(), a.c_str(), b.c_str(), imppath.c_str() );
			///////////////////////////////////
			CFile fx_file( imppath.c_str(), EFM_READ );
			OrkAssert( fx_file.IsOpen() );
			EFileErrCode eFileErr = fx_file.GetLength( scanner2.ifilelen );
			OrkAssert( scanner2.ifilelen<scanner2.kmaxfxblen );
			eFileErr = fx_file.Read( scanner2.fxbuffer, scanner2.ifilelen );
			scanner2.fxbuffer[scanner2.ifilelen] = 0;
			///////////////////////////////////
			scanner2.Scan();

			const auto& stoks = scanner2.tokens;
			auto& dtoks = scanner.tokens;

			dtoks.insert(dtoks.begin()+itokidx,stoks.begin(),stoks.end());

		}

		return pcfg;
	}
	///////////////////////////////////////////////////////////
	GlslFxStreamInterface* ParseFxInterface(GLenum iftype)
	{	
		GlslFxScanViewRegex r("(\n)",true);
		GlslFxScannerView v( scanner, r );
		v.ScanBlock(itokidx);

		////////////////////////

		GlslFxStreamInterface* psi = new GlslFxStreamInterface;
		psi->mName = v.GetBlockName();
		psi->mInterfaceType = iftype;

		////////////////////////

		const std::string BlockType = v.GetToken(v.mBlockType)->text;
		bool is_vtx = BlockType=="vertex_interface";
		bool is_geo = BlockType=="geometry_interface";

		size_t inumdecos = v.GetNumBlockDecorators();

		for( size_t ideco=0; ideco<inumdecos; ideco++ )
		{
			auto ptok = v.GetBlockDecorator(ideco);

			if( is_vtx )
			{
				auto it_vi = mpContainer->mVertexInterfaces.find(ptok->text);
				assert(it_vi!=mpContainer->mVertexInterfaces.end());
				psi->Inherit(*it_vi->second);
			}
			else if( is_geo )
			{	
				auto it_fig = mpContainer->mGeometryInterfaces.find(ptok->text);
				auto it_fiv = mpContainer->mVertexInterfaces.find(ptok->text);
				bool is_geo = (it_fig!=mpContainer->mGeometryInterfaces.end());
				bool is_vtx = (it_fiv!=mpContainer->mVertexInterfaces.end());
				assert(is_geo||is_vtx);

				auto par = is_geo 
				         ? it_fig->second 
					     : it_fiv->second;

				printf( "iface<%s> inherit<%s:%p>\n", 
					psi->mName.c_str(),
					ptok->text.c_str(), par );

				psi->Inherit(*par);
			}			
			else
			{	
				auto it_fi = mpContainer->mFragmentInterfaces.find(ptok->text);
				assert(it_fi!=mpContainer->mFragmentInterfaces.end());
				psi->Inherit(*it_fi->second);
			}			
		}

		////////////////////////

		int ist = v.mStart+1;
		int ien = v.mEnd-1;

		for( size_t i=ist; i<=ien; )
		{
			const token* vt_tok = v.GetToken(i);
			const token* dt_tok = v.GetToken(i+1);
			const token* nam_tok = v.GetToken(i+2);
			
			//printf( "  ParseFxInterface Tok<%s>\n", vt_tok->text.c_str() );

			if( vt_tok->text == "layout" )
			{
				std::string layline;
				bool done = false;
				while( false==done )
				{
					const auto& txt = vt_tok->text;

					bool add_space = (txt == "(")||(txt == ")")||(txt == ",");
					if( add_space )
						layline += " ";
					layline += vt_tok->text;
					if( add_space )
						layline += " ";
					done = vt_tok->text == ";";
					i++;
					vt_tok = v.GetToken(i);
				}
				layline += "\n";
				psi->mPreamble.push_back(layline);
			}
			else if( vt_tok->text == "uniform" )
			{
				auto it = psi->mUniforms.find(nam_tok->text);
				assert(it==psi->mUniforms.end()); // make sure there are no duplicate uniforms

				GlslFxUniform* puni = mpContainer->MergeUniform( nam_tok->text );
				puni->mTypeName = dt_tok->text;
				psi->mUniforms[ nam_tok->text ] = puni;
				if( v.GetToken(i+3)->text=="[" )
				{
					assert(v.GetToken(i+5)->text=="]");
					puni->mArraySize = atoi(v.GetToken(i+4)->text.c_str());
					printf( "uniname<%s> arraysize<%d>\n", nam_tok->text.c_str(), puni->mArraySize );
					i += 7;
					//assert(false);
				}
				else
				{
					i += 4;
				}
			}
			else if( vt_tok->text == "in" )
			{
				auto it = psi->mAttributes.find(nam_tok->text);
				assert(it==psi->mAttributes.end()); // make sure there are no duplicate attrs

				int iloc = int(psi->mAttributes.size());
				GlslFxAttribute* pattr = new GlslFxAttribute( nam_tok->text );
				pattr->mTypeName = dt_tok->text;
				pattr->mDirection = "in";


				psi->mAttributes[ nam_tok->text ] = pattr;
				if( v.GetToken(i+3)->text==":" )
				{
					pattr->mSemantic = v.GetToken(i+4)->text;
					printf( "SEMANTIC<%s>\n", pattr->mSemantic.c_str() );	
					i += 6;
				}
				else if( v.GetToken(i+3)->text==";" )
				{
					i += 4;
				}
				else if( v.GetToken(i+3)->text=="[" )
				{
					pattr->mArraySize = atoi( v.GetToken(i+4)->text.c_str() );
					i += 7;
				}
				else
				{
					assert(false);
				}
				pattr->mLocation = int(psi->mAttributes.size());

			}
			else if( vt_tok->text == "out" )
			{
				int iloc = int(psi->mAttributes.size());
				GlslFxAttribute* pattr = new GlslFxAttribute( nam_tok->text );
				pattr->mTypeName = dt_tok->text;
				pattr->mDirection = "out";
				pattr->mLocation = iloc;
				psi->mAttributes[ nam_tok->text ] = pattr;
				i += 4;
			}
			else if( vt_tok->text == "\n" )
			{
				i++;
			}
			else
			{
				printf( "invalid token<%s>\n", vt_tok->text.c_str() );
				OrkAssert(false);
			}
		}

		////////////////////////
		// sort attributes for performance
		//  (see http://stackoverflow.com/questions/16415037/opengl-core-profile-incredible-slowdown-on-os-x)
		////////////////////////

		std::multimap<int,GlslFxAttribute*> attr_sort_map;
		for( const auto& it : psi->mAttributes )
		{
			auto attr = it.second;
			auto itloc = gattrsorter.find(attr->mSemantic);
			int isort = 100;
			if( itloc!=gattrsorter.end() )
			{
				isort = itloc->second;
			}
			attr_sort_map.insert(std::make_pair(isort,attr));
			//pattr->mLocation = itloc->second;
		}

		int isort = 0;
		for( const auto& it : attr_sort_map)
		{
			auto attr = it.second;
			attr->mLocation = isort++;
		}

		////////////////////////
		itokidx = v.GetBlockEnd()+1;
		return psi;
	}
	///////////////////////////////////////////////////////////
	GlslFxStateBlock* ParseFxStateBlock()
	{
		GlslFxScanViewRegex r("(\n)",true);
		GlslFxScannerView v( scanner, r );
		v.ScanBlock(itokidx);

		GlslFxStateBlock* psb = new GlslFxStateBlock;
		psb->mName = v.GetBlockName();
		mpContainer->AddStateBlock( psb );
		//////////////////////

		auto& apptors = psb->mApplicators;

		//////////////////////

		size_t inumdecos = v.GetNumBlockDecorators();

		assert(inumdecos<2);
		
		for( size_t ideco=0; ideco<inumdecos; ideco++ )
		{
			auto ptok = v.GetBlockDecorator(ideco);
			GlslFxStateBlock* ppar = mpContainer->GetStateBlock( ptok->text );
			OrkAssert(ppar!=nullptr);
			psb->mApplicators = ppar->mApplicators;
		}

		//////////////////////

		int ist = v.mStart+1;
		int ien = v.mEnd-1;

		for( size_t i=ist; i<=ien; )
		{

			const token* vt_tok = v.GetToken(i);
			//printf( "  ParseFxStateBlock Tok<%s>\n", vt_tok.text.c_str() );
			if( vt_tok->text == "inherits" )
			{
				const token* parent_tok = v.GetToken(i+1);
				GlslFxStateBlock* ppar = mpContainer->GetStateBlock( parent_tok->text );
				OrkAssert(ppar!=nullptr);
				psb->mApplicators = ppar->mApplicators;
				i += 3;
			}
			else if( vt_tok->text == "CullTest" )
			{
				const std::string& mode = v.GetToken(i+2)->text;
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
			else if( vt_tok->text == "DepthMask" )
			{
				const std::string& mode = v.GetToken(i+2)->text;
				bool bena = (mode == "true");
				psb->AddStateFn( [=](GfxTarget*t)
				{	t->RSI()->SetZWriteMask(bena);
				} );
				printf( "DepthMask<%d>\n", int(bena) );
				i += 4;
			}
			else if( vt_tok->text == "DepthTest" )
			{
				const std::string& mode = v.GetToken(i+2)->text;
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
			else if( vt_tok->text == "BlendMode" )
			{
				const std::string& mode = v.GetToken(i+2)->text;
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
			else if( vt_tok->text == "\n" )
			{
				i++;
			}
			else
			{
				OrkAssert(false);
			}
		}
		//////////////////////
		itokidx = v.GetBlockEnd()+1;
		return psb;
	}
	///////////////////////////////////////////////////////////
	GlslFxLibBlock* ParseLibraryBlock()
	{	auto pret = new GlslFxLibBlock(scanner);
		pret->mView->ScanBlock(itokidx);
		pret->mName = scanner.tokens[ itokidx+1 ].text;
		itokidx = pret->mView->GetBlockEnd()+1;
		return pret;
	}
	///////////////////////////////////////////////////////////
	int ParseFxShaderCommon(GlslFxShader* pshader)
	{
		bool bkill = false;

		GlslFxScanViewRegex r("()",true);
		GlslFxScannerView v( scanner, r );
		v.ScanBlock(itokidx);
		//v.Dump();

		pshader->mpContainer = mpContainer;

		///////////////////////////////////
		std::string shadername = v.GetBlockName();

		GlslFxLibBlock* plibblock = nullptr;

		const token* ptok = nullptr;
		//int itok = v.mBlockName+1;

		//////////////////////////////////////////////
		// enumerate lib blocks / interfaces
		//////////////////////////////////////////////

		bool bvtx = pshader->mShaderType == GL_VERTEX_SHADER;
		bool bgeo = pshader->mShaderType == GL_GEOMETRY_SHADER;
		bool bfrg = pshader->mShaderType == GL_FRAGMENT_SHADER;

		std::vector<GlslFxLibBlock*> lib_blocks;

		GlslFxStreamInterface* iface = nullptr;

		{
			size_t inumdecos = v.GetNumBlockDecorators();

			for( size_t ideco=0; ideco<inumdecos; ideco++ )
			{
				ptok = v.GetBlockDecorator(ideco);

				auto it_lib = mpContainer->mLibBlocks.find(ptok->text);
				auto it_vi = mpContainer->mVertexInterfaces.find(ptok->text);
				auto it_fi = mpContainer->mFragmentInterfaces.find(ptok->text);
				auto it_gi = mpContainer->mGeometryInterfaces.find(ptok->text);
					
				if( it_lib != mpContainer->mLibBlocks.end() )
				{
					auto plibblock = it_lib->second;
					lib_blocks.push_back( plibblock );
					printf( "LIBBLOCK <%s>\n", ptok->text.c_str() );
				}
				else if( bvtx && it_vi != (mpContainer->mVertexInterfaces.end()) )
				{
					iface = mpContainer->GetVertexInterface( ptok->text );
					pshader->mpInterface = iface;
					printf( "VINF <%s>\n", ptok->text.c_str() );
				}
				else if( bgeo && ( it_gi != mpContainer->mGeometryInterfaces.end() ) ) 
				{	iface = mpContainer->GetGeometryInterface( ptok->text );
					pshader->mpInterface = iface;
					printf( "GINF <%s>\n", ptok->text.c_str() );
				}
				else if( bfrg && ( it_fi != mpContainer->mFragmentInterfaces.end() ) ) 
				{	iface = mpContainer->GetFragmentInterface( ptok->text );
					pshader->mpInterface = iface;
					printf( "FINF <%s>\n", ptok->text.c_str() );
				}
				else
				{
					printf( "bad shader interface decorator!\n");
					printf( "shader<%s>\n", shadername.c_str() );
					printf( "deco<%s>\n", ptok->text.c_str() );
					printf( "is_vtx<%d> is_geo<%d> is_frg<%d>\n", int(bvtx), int(bgeo), int(bfrg) );
					assert(false);
				}
			}
		}

		//////////////////////////////////////////////

		assert( iface!=nullptr );

		//////////////////////////////////////////////
		//printf( "ParseFxShaderCommon Eob<%d> Next<%s>\n", iend, etok.text.c_str() );
		///////////////////////////////////

		std::string shaderbody;

		size_t iline = 0;
		FixedString<64> fxstr;
		auto prline = [&]()
		{
			fxstr.format("/*%03d*/", int(iline) );
			shaderbody += fxstr.c_str();
			iline++;
		};

		prline();

		shaderbody += "#version 330 core\n";

		for( const auto& preamble_line : iface->mPreamble )
		{
			prline();
			shaderbody += preamble_line;
		}

		///////////////////////
		// UNIFORMS
		///////////////////////

		for( GlslFxStreamInterface::UniMap::const_iterator
			itu=iface->mUniforms.begin();
			itu!=iface->mUniforms.end();
			itu++ )
		{
			prline();
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
		///////////////////////
		// ATTRIBUTES
		///////////////////////
		for( GlslFxStreamInterface::AttrMap::const_iterator
			ita=iface->mAttributes.begin();
			ita!=iface->mAttributes.end();
			ita++ )
		{
			prline();
			GlslFxAttribute* pa = ita->second;

			shaderbody += pa->mDirection + " ";
			shaderbody += pa->mTypeName + " ";

			if( pa->mArraySize )
			{
				ork::FixedString<128> fxs;
				fxs.format("%s[%d]", pa->mName.c_str(), pa->mArraySize );
				shaderbody += fxs.c_str();
			}
			else
				shaderbody += pa->mName;

			shaderbody += ";";

			if( pa->mComment.length() )
			{	shaderbody += pa->mComment;
				bkill = true;
			}

			shaderbody += "\n";
		}
		///////////////////////////////////
		auto code_inject = [&](const GlslFxScannerView& view)
		{

			int ist = view.mStart+1;
			int ien = view.mEnd-1;

			bool bnewline = true;

			size_t column = 0;
			int indent = 1;

			for( size_t i=ist; i<=ien; i++ )
			{
				ptok = view.GetToken(i);

				if( bnewline )
				{
					prline();

					for( int in=0; in<indent; in++ )
						shaderbody += "\t";
					column = 0;

				}

				const std::string& cur_tok = ptok->text;
				//printf( "  ParseFxShaderCommon Tok<%s>\n", cur_tok.c_str() );
				shaderbody += cur_tok;

				//if( column < 2 )
					shaderbody += " ";

				column++;

				bnewline = false;
				if( cur_tok=="\n" )
				{	bnewline = true;
				}
				else if( cur_tok=="{" )
					indent++;
				else if( cur_tok=="}" )
					indent--;
			}
		};


		///////////////////////////////////
		// inject libblock code
		///////////////////////////////////
		for( const auto& libblk : lib_blocks )
		{
			prline();
			shaderbody += "// libblock<" + libblk->mName + "> ///////////////////////////////////\n";

			const GlslFxScannerView& lib_view = *libblk->mView;
			//printf( "LibBlockView.Start<%d> LibBlockView.End<%d> scanner.numtoks<%d> view.numtoks<%d>\n", view.mStart,view.mEnd, int(scanner.tokens.size()), int(view.mIndices.size()) );
			code_inject(lib_view);

			shaderbody += "///////////////////////////////////////////////////////////////////\n";
		}	
		///////////////////////////////////
		prline();
		shaderbody += "void " + shadername + "()\n{";

		size_t iblockstart = v.mStart;
		size_t iblockend = v.mEnd;

		code_inject( v );

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
		int new_end = v.GetBlockEnd()+1;
		printf( "newend be<%d> deref<%d>\n", int(iblockend), new_end );

		//assert(false==bkill);

		return new_end;
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
	GlslFxShaderGeo* ParseFxGeometryShader()
	{
		GlslFxShaderGeo* pshader = new GlslFxShaderGeo();
		itokidx = ParseFxShaderCommon( pshader );
		return pshader;
	}
	///////////////////////////////////////////////////////////
	GlslFxTechnique* ParseFxTechnique()
	{
		GlslFxScanViewRegex r("(\n)",true);
		GlslFxScannerView v( scanner, r );
		v.ScanBlock(itokidx);

		//int iend = FindEndOfBlock( itokidx+1, itokidx+2 );
		//const token& etok = scanner.tokens[iend+1];
		//printf( "ParseFxTechnique Eob<%d> Next<%s>\n", iend, etok.text.c_str() );

		std::string tekname = v.GetBlockName();

		GlslFxTechnique* ptek = new GlslFxTechnique( tekname );
		////////////////////////////////////////
		//OrkAssert( scanner.tokens[itokidx+2].text == "{" );
		////////////////////////////////////////

		int ist = v.mStart+1;
		int ien = v.mEnd-1;

		for( int i=ist; i<=ien; )
		{
			const token* vt_tok = v.GetToken(i);
			//printf( "  ParseFxTechnique Tok<%s>\n", vt_tok.text.c_str() );
			if( vt_tok->text == "fxconfig" )
			{
				i+=4;
			}
			else if( vt_tok->text == "pass" )
			{
				printf( "parsing pass at i<%d>\n", i );
				// i is in view space, we need the globspace index to 
				//  start the pass parse
				int globspac_passtoki = v.GetTokenIndex(i); 
				i = ParseFxPass(globspac_passtoki,ptek);
			}
			else if( vt_tok->text == "\n" )
			{
				i++;
			}
			else
			{
				OrkAssert(false);
			}
		}
		////////////////////////////////////////
		itokidx = v.GetBlockEnd()+1;
		return ptek;
	}
	///////////////////////////////////////////////////////////
	int ParseFxPass( int istart, GlslFxTechnique* ptek )
	{
		GlslFxScanViewRegex r("(\n)",true);
		GlslFxScannerView v( scanner, r );
		v.ScanBlock(istart);

		std::string name = v.GetBlockName();
		//printf( "ParseFxPass Name<%s> Eob<%d> Next<%s>\n", name.c_str(), iend, etok.text.c_str() );
		GlslFxPass* ppass = new GlslFxPass( name );
		////////////////////////////////////////
		//OrkAssert( scanner.tokens[istart+2].text == "{" );
		/////////////////////////////////////////////

		int ist = v.mStart+1;
		int ien = v.mEnd-1;

		for( size_t i=ist; i<=ien; )
		{
			const token* vt_tok = v.GetToken(i);
			//printf( "  ParseFxPass Tok<%s>\n", vt_tok->text.c_str() );
			
			if( vt_tok->text == "vertex_shader" )
			{
				std::string vsnam = v.GetToken(i+2)->text;
				GlslFxShader* pshader = mpContainer->GetVertexProgram( vsnam );
				OrkAssert( pshader != nullptr );
				ppass->mVertexProgram = pshader;
				i+=4;
			}
			else if( vt_tok->text == "fragment_shader" )
			{
				std::string fsnam = v.GetToken(i+2)->text;
				GlslFxShader* pshader = mpContainer->GetFragmentProgram( fsnam );
				OrkAssert( pshader != nullptr );
				ppass->mFragmentProgram = pshader;
				i+=4;
			}
			else if( vt_tok->text == "geometry_shader" )
			{
				std::string fsnam = v.GetToken(i+2)->text;
				GlslFxShader* pshader = mpContainer->GetGeometryProgram( fsnam );
				OrkAssert( pshader != nullptr );
				ppass->mGeometryProgram = pshader;
				i+=4;
			}
			else if( vt_tok->text == "state_block" )
			{
				std::string sbnam = v.GetToken(i+2)->text;
				GlslFxStateBlock* psb = mpContainer->GetStateBlock( sbnam );
				OrkAssert(psb!=nullptr);
				ppass->mStateBlock = psb;
				i+=4;
			}
			else if( vt_tok->text == "\n" )
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
		return v.GetBlockEnd()+1;
	}
	///////////////////////////////////////////////////////////
	void DumpAllTokens()
	{	size_t itokidx = 0;
		const std::vector<token>& tokens = scanner.tokens;
		while( itokidx<tokens.size() )
		{
			const token& tok = tokens[itokidx++];
			//printf( "tok<%d> <%s>\n", itokidx, tok.text.c_str() );
		}
	}
	///////////////////////////////////////////////////////////
	GlslFxContainer* Parse( const std::string& fxname)
	{
		const std::vector<token>& tokens = scanner.tokens;
		
		//printf( "NumTokens<%d>\n", int(tokens.size()) );
		
		mpContainer = new GlslFxContainer( fxname.c_str() );
		bool bOK = true;
		
		itokidx = 0;

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
				GlslFxStreamInterface* pif = ParseFxInterface(GL_VERTEX_SHADER);
				mpContainer->AddVertexInterface( pif );
			}
			else if( tok.text == "fragment_interface" )
			{
				GlslFxStreamInterface* pif = ParseFxInterface(GL_FRAGMENT_SHADER);
				mpContainer->AddFragmentInterface( pif );
			}
			else if( tok.text == "geometry_interface" )
			{
				GlslFxStreamInterface* pif = ParseFxInterface(GL_GEOMETRY_SHADER);
				mpContainer->AddGeometryInterface( pif );
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
			else if( tok.text == "geometry_shader" )
			{
				GlslFxShaderGeo* pshader = ParseFxGeometryShader();
				mpContainer->AddGeometryProgram( pshader );
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

GlslFxLibBlock::GlslFxLibBlock( const GlslFxScanner& s )
	: mFilter(nullptr)
	, mView(nullptr)
{
	mFilter =  new GlslFxScanViewFilter();
	mView = new GlslFxScannerView( s, *mFilter );
}


//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

GlslFxContainer* LoadFxFromFile( const AssetPath& pth )
{
	GlslFxScanner scanner;
	GlSlFxParser parser(pth,scanner);
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
