
//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "SignalTest.h"

#include "TestUtil.h"
#include "react/react.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////
namespace
{

using namespace react;

using P1 = DomainParams;

INSTANTIATE_TYPED_TEST_CASE_P( SeqToposort, SignalTest, P1 );

} // namespace