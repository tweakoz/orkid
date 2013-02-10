#include <ork/pch.h>
#include <ork/kernel/fixedstring.h>
#include <unittest++/UnitTest++.h>
#include <string.h>
#include <unordered_set>
#include <set>

using namespace ork;

TEST(fixedstringTestFormat)
{
	FixedString<4096> the_string;
	the_string.format( "what<%d>up<%d>%s", 0, 1, "yo" );
	CHECK(0==strcmp(the_string.c_str(),"what<0>up<1>yo"));
}

TEST(fixedstringTestReplace1)
{
	FixedString<4096> src_string("whatupyodiggittyyoyo");
	FixedString<4096> dst_string;

	dst_string.replace( src_string.c_str(), "yo", "damn" );
	//printf( "dst_string<%s>\n", dst_string.c_str() );
	CHECK(0==strcmp(dst_string.c_str(),"whatupdamndiggittydamndamn"));
}

TEST(fixedstringTestFind1)
{
	FixedString<4096> src_string("whatupyodiggittyyoyo");
	size_t f = src_string.find_first_of( "diggit" );
	CHECK(f==size_t(8));
}

TEST(fixedstringTestFind2)
{
	FixedString<4096> src_string("whatupyodiggittyyoyo");
	size_t f = src_string.find_first_of( "yo" );
	CHECK(f==size_t(6));
}

TEST(fixedstringTestFind3)
{
	FixedString<4096> src_string("whatupyodiggittyyoyo");
	size_t f = src_string.find_last_of( "yo" );
	CHECK(f==size_t(18));
}

TEST(fixedstringTestSize1)
{
	FixedString<4> src_string("wha");
	CHECK(src_string.size()==3);
}

TEST(fixedstringTestSize2)
{
	FixedString<4> src_string;
	src_string.format("what");
	CHECK(src_string.size()==3); // 3 because it will get truncated due to length constraints
}

TEST(fixedstringHashSetCompare1)
{
	std::unordered_set<FixedString<256u>> the_set;
	the_set.insert("yo");
	the_set.insert("what");
	the_set.insert("up");
	the_set.insert("yo");
	the_set.insert("what");
	the_set.insert("up");
	CHECK(the_set.size()==3); 
}

TEST(fixedstringOrderedSetCompare1)
{
	std::set<FixedString<256>> the_set;
	the_set.insert("yo");
	the_set.insert("what");
	the_set.insert("up");
	the_set.insert("yo");
	the_set.insert("what");
	the_set.insert("up");
	CHECK(the_set.size()==3); 
}

TEST(fixedstringMixedConcat)
{
	FixedString<32> a("whatup");
	FixedString<64> b("yo");

	a += b;

	CHECK(0==strcmp(a.c_str(),"whatupyo")); 
}
