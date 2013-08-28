#pragma once

#include <regex>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
/////////////////////////////////////////////////////////////////////////////////////////////////

struct token
{
	int iline;
	int icol;
	std::string text;
	token( const std::string& txt, int il, int ic ) : text(txt), iline(il), icol(ic) {}
};

typedef enum scan_state
{
	ESTA_NONE = 0,
	ESTA_C_COMMENT,
	ESTA_CPP_COMMENT,
	ESTA_DQ_STRING,
	ESTA_SQ_STRING,
	ESTA_WSPACE,
	ESTA_CONTENT
	
};

struct GlslFxScanner
{
	GlslFxScanner();
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
	void AddToken( const token& tok );
	void Scan();
	/////////////////////////////////////////
	/////////////////////////////////////////
	static const int kmaxfxblen = 64<<10;
	char fxbuffer[ kmaxfxblen ];
	size_t ifilelen;
	std::vector<token> tokens;
	token cur_token;
	scan_state ss;
};

struct GlslFxScanViewFilter
{
	virtual bool Test(const token& t) { return true; }
};
struct GlslFxScanViewRegex : public GlslFxScanViewFilter
{
	GlslFxScanViewRegex(const char*, bool inverse);

	bool Test(const token& t) override;

	std::regex mRegex;
	bool mInverse;
};

struct GlslFxScanRange
{
	GlslFxScanRange() : mStart(0), mEnd(0) {}
	size_t mStart;
	size_t mEnd;
};

struct GlslFxScannerView
{
	GlslFxScannerView( const GlslFxScanner& s, GlslFxScanViewFilter& f  );

	size_t GetNumTokens() const { return mIndices.size(); }
	const token* GetToken(size_t i) const;
	void ScanRange( size_t is, size_t ie );
	void ScanBlock( size_t is );
	void Dump();


	std::vector<int> mIndices;
	GlslFxScanViewFilter& mFilter;
	const GlslFxScanner& mScanner;
	std::regex mBlockTerminators;

	size_t mStart;
	size_t mEnd;
	size_t mBlockType;
	size_t mBlockName;
	bool   mBlockOk;

};
/////////////////////////////////////////////////////////////////////////////////////////////////
}} //namespace ork { namespace lev2 {
/////////////////////////////////////////////////////////////////////////////////////////////////
