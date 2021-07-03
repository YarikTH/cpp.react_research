
//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "OperationsTest.h"

#include "TestUtil.h"
#include "react/react.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////
namespace
{

using namespace react;

using P1 = DomainParams<sequential, ToposortEngine>;

INSTANTIATE_TYPED_TEST_CASE_P( SeqToposort, OperationsTest, P1 );

} // namespace