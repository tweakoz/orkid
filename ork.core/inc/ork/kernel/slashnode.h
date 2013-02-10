////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

///////////////////////////////////////////////////////////////////////////////

namespace ork
{

void breakup_slash_path( std::string str, orkvector< std::string > &outvec );

/// Returns the first occurence of a character within a string.
///
/// @param str The string to search
/// @param cch The character to search for
/// @param start The index at which to start the search
/// @return The index of cch within str (starting at start). If cch is not found, return std::string::npos.
std::string::size_type str_cue_to_char(const std::string& str, char cch, int start);

///////////////////////////////////////////////////////////////////////////////

class SlashTree;

class SlashNode
{
	friend class SlashTree;

	std::string name;
	SlashNode *parent;
	orkmap< std::string, SlashNode * >	children_map;
	void *data;

public: //
		
	void add_child( SlashNode *child );
	std::string getfullpath( void ) const;
	SlashNode();
	~SlashNode();

	int GetNumChildren() const { return int(children_map.size()); }
	const orkmap< std::string, SlashNode * >& GetChildren() const { return children_map; }
	const std::string & GetNodeName() const { return name; }
	bool IsLeaf( void ) const { return (0==GetNumChildren()); }
	void dump( void );

	void SetData( void* pdata ) { data=pdata; }
	const void* GetData( void ) const { return data; }

	void GetPath( orkvector< const SlashNode * >& pth ) const;
};

///////////////////////////////////////////////////////////////////////////////

class SlashTree
{

	SlashNode *root;

	public: //

	SlashNode *add_node( const char* instr, void *ndata );
	void remove_node( SlashNode*pnode );
	SlashNode *find_node( const std::string & instr ) const;
	SlashTree();
	void Clear( void );
	void dump( void ) const;

	const SlashNode *GetRoot( void ) const { return root; }

};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork

