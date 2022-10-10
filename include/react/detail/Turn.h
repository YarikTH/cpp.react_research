
//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_TURN_H_INCLUDED
#define REACT_DETAIL_TURN_H_INCLUDED

#pragma once

#include "react/detail/Defs.h"

/***************************************/ REACT_IMPL_BEGIN /**************************************/

using TurnIdT = uint;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Turn
///////////////////////////////////////////////////////////////////////////////////////////////////
class Turn
{
public:
    inline Turn(TurnIdT id) :
        id_( id )
    {}

    inline TurnIdT Id() const { return id_; }

private:
    TurnIdT    id_;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_TURN_H_INCLUDED