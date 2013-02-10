////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/kernel/string/ResizableString.h>
#include <ork/kernel/string/string.h>
#include <ork/kernel/tempstring.h>

namespace ork {

void SplitString( const FixedString<256>& instr, orkvector< FixedString<64> >& splitvect, const char *pdelim )
{
	if( instr.length() )
	{
		const char* psrc = instr.c_str();

		size_t istrlen = instr.length();

		char* buffer = new char[ istrlen+1024 ];
		memset( buffer, 0, istrlen+1024 );
		strcpy( buffer, instr.c_str() );
		char *tok = strtok( buffer, pdelim );

		splitvect.push_back( tok );

		while( tok != 0 )
		{
			size_t ipos = (tok-buffer);

			tok = strtok( 0, pdelim );
			if( tok )
			{
				splitvect.push_back( tok );
			}
		}
		delete[] buffer;
	}
}

void SplitString( const std::string &instr, orkvector<std::string> &splitvect, const char *pdelim )
{
	if( instr.length() )
	{
		const char* psrc = instr.c_str();

		size_t istrlen = instr.length();

		char* buffer = new char[ istrlen+1024 ];
		memset( buffer, 0, istrlen+1024 );
		strcpy( buffer, instr.c_str() );
		char *tok = strtok( buffer, pdelim );

		splitvect.push_back( tok );

		while( tok != 0 )
		{
			size_t ipos = (tok-buffer);

			tok = strtok( 0, pdelim );
			if( tok )
			{
				splitvect.push_back( tok );
			}
		}
		delete[] buffer;
	}
}

void TokenizeString(PieceString stringToTokenize, ConstString delimiters, orkvector<PieceString>& tokenVector)
{
     PieceString::size_type startpoint = 0;
     PieceString::size_type i;

     if(stringToTokenize.empty())
          return;

     do
     {
          i = stringToTokenize.find_first_of(delimiters.c_str(), startpoint);
          PieceString token = stringToTokenize.substr(startpoint, i - startpoint);
          if(!token.empty())
               tokenVector.push_back(token);
          startpoint = i + 1;
     } while(i != ork::PieceString::npos);

}

void SplitString(const PieceString &instr,
				 orkvector<PieceString> &splitvect,
				 const ConstString &pdelim)
{
     PieceString::size_type startpoint = 0;
     PieceString::size_type i;

     if(instr.empty())
          return;

     do
     {
          i = instr.find_first_of(pdelim.c_str(), startpoint);
          PieceString token = instr.substr(startpoint, i - startpoint);
          if(!token.empty())
               splitvect.push_back(token);
          startpoint = i + 1;
     } while(i != ork::PieceString::npos);
}

tokenlist CreateTokenList( const PieceString &instr, const ConstString &pdelim )
{
	tokenlist rval;

     PieceString::size_type startpoint = 0;
     PieceString::size_type i;

     if(instr.empty())
          return rval;

     do
     {
          i = instr.find_first_of(pdelim.c_str(), startpoint);
          PieceString token = instr.substr(startpoint, i - startpoint);
          if(!token.empty())
		  {
			  std::string str( token.data(), token.length() );
			  rval.push_back(str);
		  }
          startpoint = i + 1;
     } while(i != ork::PieceString::npos);

	return rval;
}

std::string CreateFormattedString( const char* formatstring, ... )
{
	std::string rval;
	char formatbuffer[512];

	va_list args;
	va_start(args, formatstring);
	//buffer.vformat(formatstring, args);
	vsnprintf( &formatbuffer[0], sizeof(formatbuffer), formatstring, args );
	va_end(args);
	rval = formatbuffer;
	return rval;
}

std::string FormatString( const char* formatstring, ... )
{
	std::string rval;
	char formatbuffer[512];

	va_list args;
	va_start(args, formatstring);
	//buffer.vformat(formatstring, args);
	vsnprintf( &formatbuffer[0], sizeof(formatbuffer), formatstring, args );
	va_end(args);
	rval = formatbuffer;
	return rval;
}

}
