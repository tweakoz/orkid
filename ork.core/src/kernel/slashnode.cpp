////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/kernel/slashnode.h>
#include <ork/file/path.h>

namespace ork {

///////////////////////////////////////////////////////////////////////////////
// slashnode
//	separate a std::string in the form "this/is/a/slash/path"
//	into subpaths "this" "is" "a" "slash" "path"
//	note that spaces are legal!
///////////////////////////////////////////////////////////////////////////////

std::string::size_type str_cue_to_char( const std::string &str, char cch, int start )
{	std::string::size_type slen = str.size();
	bool found = false;
	std::string::size_type idx = std::string::size_type(start);
	std::string::size_type rval = std::string::npos;
	for(std::string::size_type i = idx; ((i<slen)&&(found==false)); i++)
	{
		if(cch == str[i])
		{
			found = true;
			rval = i;
		}
	}
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

void breakup_slash_path( std::string str, orkvector< std::string > &outvec )
{
	file::Path AsPath( str.c_str() );

	outvec.clear();
	if( AsPath.HasUrlBase() )
	{
		outvec.push_back( AsPath.GetUrlBase().c_str() );
	}
	if( AsPath.HasDrive() )
	{
		outvec.push_back( AsPath.GetDrive().c_str() );
	}
	if( AsPath.HasFolder() )
	{
		const std::string str2 = AsPath.GetFolder( ork::file::Path::EPATHTYPE_POSIX ).c_str();

		const std::string delims( "/" );
		int word = 0;
		bool bDone = false;
		std::string::size_type ilen = str2.size();
		std::string::size_type idx = 0;
		while( (idx < ilen) && (!bDone) )
		{
			if( str2[idx] == '/' )
			{
				outvec.push_back( "/" );
				idx++;
			}
			else
			{
				std::string::size_type Nidx = str_cue_to_char(str2, '/', int(idx + 1));
				bool bAnotherSlash = (Nidx!=-1);
				std::string::size_type len = Nidx - idx;
				std::string newword = str2.substr( idx, len );

				if( newword.length() )
				{
					outvec.push_back( newword );
				}
				idx = Nidx;
				word++;
			}
		}
	}
	if( AsPath.HasFile() )
	{
		outvec.push_back( AsPath.GetName().c_str() );
	}
	if( AsPath.HasExtension() )
	{
		outvec[outvec.size() - 1] += '.';
		outvec[outvec.size() - 1] += AsPath.GetExtension().c_str();
	}
}

///////////////////////////////////////////////////////////////////////////////

SlashNode::~SlashNode()
{
	for(orkmap< std::string, SlashNode * >::const_iterator it = children_map.begin(); it != children_map.end(); it++)
	{
		SlashNode *pnode = it->second;

		delete pnode;
	}
}

///////////////////////////////////////////////////////////////////////////////

SlashNode::SlashNode()
	: name("default")
	, parent(0)
	, data(0)
{
}

///////////////////////////////////////////////////////////////////////////////

void SlashNode::dump( void )
{
	orkvector< std::string > path_vect;

	SlashNode *par = this;

	while( par != 0 )
	{
		path_vect.push_back( par->name );
		par = par->parent;
	}

	size_t npel = path_vect.size();
	for( size_t i=0; i<npel; i++ )
	{
		size_t j = npel-(i+1);
		char *str = (char *) path_vect[j].c_str();
		if( strlen( str ) != 0 )
			orkprintf( "%s", str );
	}

	orkprintf( "\n" );

	for(	orkmap< std::string, SlashNode * >::iterator it = children_map.begin();
			it != children_map.end();
			it ++ )
	{
		std::pair< const std::string, SlashNode * > pair = *it;
		pair.second->dump();
	}
}

///////////////////////////////////////////////////////////////////////////////

void SlashNode::add_child( SlashNode *child )
{
	children_map[ child->name ] = child;
	child->parent = this;
}

///////////////////////////////////////////////////////////////////////////////

std::string SlashNode::getfullpath() const
{
	std::string rval;
	orkvector<const SlashNode*> hier;
	GetPath( hier );
	int inumnodes = int(hier.size());
	for(int i=0; i<inumnodes; i++ )
	{
		rval += hier[i]->name;
	}
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

void SlashNode::GetPath( orkvector< const SlashNode * >& pth ) const
{
	orkvector<const SlashNode*> revpth;
	const SlashNode* cur = this;
	while(cur)
	{
		revpth.push_back(cur);
		cur = cur->parent;
	}
	int inumnodes = int(revpth.size());
	pth.resize(inumnodes);
	for(int i=0; i<inumnodes; i++ )
	{
		pth[i] = revpth[ (inumnodes-1)-i ];
	}
}

///////////////////////////////////////////////////////////////////////////////

void SlashTree::remove_node( SlashNode *pnode )
{
	bool bremoved = false;

	SlashNode*pparent = pnode->parent;

	if( pparent )
	{

		for(	orkmap< std::string, SlashNode * >::iterator it = pparent->children_map.begin();
				it != pparent->children_map.end();
				it ++ )
		{
			SlashNode * tnode = it->second;

			if( tnode==pnode )
			{
				OrkSTXRemoveFromMap( pparent->children_map, pnode->name );
				delete tnode;
				bremoved = true;
				break;
			}

		}

		if( bremoved )
		{
			int inumchildren = int(pparent->children_map.size());

			if( 0 == inumchildren )
			{
				void *pdata = pparent->data;

				remove_node( pparent );
			}
		}
	}
		
}

///////////////////////////////////////////////////////////////////////////////

SlashNode *SlashTree::add_node( const char* instr, void *ndata )
{
	SlashNode *rval = 0;
	
	orkvector< std::string > parsed_path;
	breakup_slash_path( instr, parsed_path );

	///////////////////////////////////
	//	search for deepest node found
	
	bool found = true;
	
	size_t Nppath = parsed_path.size();
	SlashNode *ptr = root;

	for( size_t idx=0; idx<Nppath; idx++ )
	{
		std::string mstr = parsed_path[ idx ];
		orkmap< std::string, SlashNode * >::iterator it = ptr->children_map.find( mstr );
		if( it != ptr->children_map.end() )
		{	std::pair< const std::string, SlashNode * > *pair = &(*it);
			ptr = pair->second;
		}
		else // not found
		{
			SlashNode *nnod = new SlashNode;
			nnod->name = mstr;
			ptr->children_map[ mstr ] = nnod;
			nnod->parent = ptr;
			rval = nnod;

			//orkprintf( "added node %08x %s parent %08x %s (Data %08x)\n", nnod, mstr.c_str(), ptr, ptr->name.c_str(), ndata );

			nnod->data = ndata;
			ptr = nnod;
		}
		
	}

	/////////////////////
	
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

void SlashTree::Clear( void )
{
	if( root )
	{
		delete root;
	}
	root = new SlashNode;
	root->name = "";
	
}

///////////////////////////////////////////////////////////////////////////////

SlashTree::SlashTree()
	: root( 0 )
{
	Clear();
}

///////////////////////////////////////////////////////////////////////////////

void SlashTree::dump( void ) const
{
	orkprintf( "////////////////////////////////////////\n" );
	orkprintf( "dumping slashnode hierarchy %p\n", this );
	root->dump();
	orkprintf( "////////////////////////////////////////\n" );
}

} // namespace ork
