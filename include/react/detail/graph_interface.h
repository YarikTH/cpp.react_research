
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
class node_id
{
public:
    using value_type = size_t;

    node_id() = default;

    explicit node_id(value_type id) : m_id(id){}

    operator value_type()
    {
        return m_id;
    }

    bool operator==( node_id other) const noexcept
    {
        return m_id == other.m_id;
    }

    bool operator!=(node_id other) const noexcept
    {
        return m_id != other.m_id;
    }
private:
    value_type m_id = -1;
};

enum class update_result
{
    unchanged,
    changed,
    shifted
};

class react_graph;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// reactive_node_interface
///////////////////////////////////////////////////////////////////////////////////////////////////
struct reactive_node_interface
{
    virtual ~reactive_node_interface() = default;

    virtual update_result update() noexcept = 0;
    
    virtual void finalize() noexcept
        { }
};



/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_GRAPH_INTERFACE_H_INCLUDED