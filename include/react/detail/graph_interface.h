
//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_GRAPH_INTERFACE_H_INCLUDED
#define REACT_DETAIL_GRAPH_INTERFACE_H_INCLUDED

#pragma once

#include "react/detail/defs.h"

#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>

#include "react/api.h"
#include "react/common/utility.h"

/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Definitions
///////////////////////////////////////////////////////////////////////////////////////////////////
using NodeId = size_t;

enum class UpdateResult
{
    unchanged,
    changed,
    shifted
};

class react_graph;
struct IReactNode;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IReactNode
///////////////////////////////////////////////////////////////////////////////////////////////////
struct IReactNode
{
    virtual ~IReactNode() = default;

    virtual UpdateResult Update() noexcept = 0;
    
    virtual void finalize() noexcept
        { }
};



/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_GRAPH_INTERFACE_H_INCLUDED