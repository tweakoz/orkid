//
// https://github.com/SteveKChiu/lua-intf
//
// Copyright 2014, Steve K. Chiu <steve.k.chiu@gmail.com>
//
// The MIT License (http://www.opensource.org/licenses/mit-license.php)
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//

#ifndef LUAINTF_H
#define LUAINTF_H

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmacro-redefined"

#include <type_traits>
#include <cstdint>

#if defined(__APPLE__)
namespace std{
template <class> struct result_of;

template <class F, class... TN>
struct result_of<F(TN...)>
{
    using type = ::std::invoke_result_t<F, TN...>;
};

template<class... TN> using result_of_t = typename result_of<TN...>::type;    

using intmax_t = ::intmax_t;
}
#endif

//---------------------------------------------------------------------------

#include "LuaContext.h"

namespace LuaIntf
{

//---------------------------------------------------------------------------

#include "CppArg.h"
#include "CppInvoke.h"
#include "CppObject.h"
#include "CppBindModule.h"
#include "CppBindClass.h"
#include "CppFunction.h"

#if LUAINTF_HEADERS_ONLY
#include "CppBindModule.cpp"
#include "CppBindClass.cpp"
#include "CppObject.cpp"
#include "CppFunction.cpp"
#endif

//---------------------------------------------------------------------------

#pragma GCC diagnostic pop

}

#endif
