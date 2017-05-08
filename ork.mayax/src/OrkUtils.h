///////////////////////////////////////////////////////////////////////////////
// Ork Maya Utils
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>
#include <set>
#include <map>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace maya {
/////////////////////////////////////////////////////////////////////
std::string formatString( const char *formatstring, ... );
void logMessage( const char *formatstring, ... );
void logError( const char *formatstring, ... );
/////////////////////////////////////////////////////////////////////
template <typename T>
bool itemInSet(const std::set<T>& the_set, const T& item)
{
    return (the_set.end()!=the_set.find(item));
}
/////////////////////////////////////////////////////////////////////
// uniquevect : part vector, part set...
/////////////////////////////////////////////////////////////////////
template <typename T> struct uniquevect
{
    int merge(const T& item)
    {
        int rval = -1;
        if( ! itemInSet(_set,item) )
        {   rval = _array.size();
            _array.push_back( item );
            _set.insert( item );
        }
        return rval;     
    }
    T itemAtIndex(int idx) const
    {   return _array[idx];
    }
    size_t size() const { return _array.size(); }

    std::set<T>     _set;
    std::vector<T>  _array;
};
/////////////////////////////////////////////////////////////////////
}} //namespace ork { namespace maya {


