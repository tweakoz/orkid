#pragma once

#include <regex>


/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////

struct Token
{
	int iline;
	int icol;
	std::string text;
	Token( const std::string& txt, int il, int ic ) : text(txt), iline(il), icol(ic) {}
};

enum scan_state
{
	ESTA_NONE = 0,
	ESTA_C_COMMENT,
	ESTA_CPP_COMMENT,
	ESTA_DQ_STRING,
	ESTA_SQ_STRING,
	ESTA_WSPACE,
	ESTA_CONTENT

};

struct Scanner
{
	Scanner();
	/////////////////////////////////////////
	char to_lower( char ch ) { return ((ch>='A')&&(ch<='Z')) ? (ch-'A'+'a') : ch; }
	/////////////////////////////////////////
	bool is_alf( char ch ) { return (to_lower(ch)>='a')&&(to_lower(ch<='z')); }
	/////////////////////////////////////////
	bool is_num( char ch ) { return (ch>='0')&&(ch<='9'); }
	/////////////////////////////////////////
	bool is_alfnum( char ch ) { return is_alf(ch)||is_num(ch); }
	/////////////////////////////////////////
	bool is_spc( char ch ) { return (ch==' ')||(ch==' ')||(ch=='\t'); }
	/////////////////////////////////////////
	bool is_septok( char ch )
	{	return
		(ch==';')||(ch==':')||(ch=='{')||(ch=='}')
		||	(ch=='[')||(ch==']')||(ch=='(')||(ch==')')
		||	(ch=='*')||(ch=='+')||(ch=='-')||(ch=='=')
		||  (ch==',')||(ch=='?')||(ch=='%')
		||	(ch=='<')||(ch=='>')||(ch=='&')||(ch=='|')
		||	(ch=='!')||(ch=='/')
		;
	}
	/////////////////////////////////////////
	bool is_content( char ch ) { return is_alfnum(ch)||(ch=='_')||(ch=='.'); }
	/////////////////////////////////////////
	void FlushToken();
	void AddToken( const Token& tok );
	void Scan();
	/////////////////////////////////////////
	const Token* token(size_t i) const;
	/////////////////////////////////////////
	static const int kmaxfxblen = 64<<10;
	char fxbuffer[ kmaxfxblen ];
	size_t ifilelen;
	std::vector<Token> tokens;
	Token cur_token;
	scan_state ss;
};

struct ScanViewFilter
{
	virtual bool Test(const Token& t) { return true; }
};
struct ScanViewRegex : public ScanViewFilter
{
	ScanViewRegex(const char*, bool inverse);

	bool Test(const Token& t) override;

	std::regex mRegex;
	bool mInverse;
};

struct ScanRange
{
	ScanRange() : mStart(0), mEnd(0) {}
	size_t mStart;
	size_t mEnd;
};

struct ScannerView
{
	ScannerView( const Scanner& s, ScanViewFilter& f  );
	ScannerView( const ScannerView& oth,int start_offset  );

	size_t numTokens() const { return _indices.size(); }
	const Token* token(size_t i) const;
	size_t tokenIndex(size_t i) const;
	void scanBlock( size_t is, bool checkterm=true, bool checkdecos=true );
	void dump();
	size_t blockEnd() const;
	std::string blockName() const;

	const int numBlockDecorators() const { return _blockDecorators.size(); }
	const Token* blockDecorator(size_t i) const;

	std::vector<int> _indices;
	ScanViewFilter& _filter;
	const Scanner& _scanner;
	std::regex _blockTerminators;

	size_t _start; // will point to lev0 { if exists in blockmode
	size_t _end; // will point to lev0 } if exists in blockmode
	std::vector<int> _blockDecorators;
	size_t _blockType;
	size_t _blockName;
	bool   _blockOk;

};
/////////////////////////////////////////////////////////////////////////////////////////////////
} //namespace ork { namespace lev2 {
/////////////////////////////////////////////////////////////////////////////////////////////////
