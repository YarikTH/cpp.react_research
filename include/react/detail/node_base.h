
//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_NODE_BASE_H_INCLUDED
#define REACT_DETAIL_NODE_BASE_H_INCLUDED

#pragma once

#include "react/detail/defs.h"

#include <iterator>
#include <memory>
#include <utility>

#include "react/common/utility.h"
#include "react/detail/graph_interface.h"

/***************************************/ REACT_IMPL_BEGIN /**************************************/

class react_graph;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// create_wrapped_node
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename RET, typename NODE, typename ... ARGS>
RET create_wrapped_node(ARGS&& ... args)
{
    return RET(std::make_shared<NODE>(std::forward<ARGS>(args)...));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// NodeBase
///////////////////////////////////////////////////////////////////////////////////////////////////
class node_base : public reactive_node_interface
{
public:
    node_base(const context& group) :
        group_( group ),
        nodeId_( get_graph().register_node(this) )
    {
    }

    ~node_base() override
    {
        get_graph().unregister_node(nodeId_);
    }

    node_base(const node_base&) = delete;
    node_base& operator=(const node_base&) = delete;

    node_base( node_base&&) = delete;
    node_base& operator=( node_base&&) = delete;

    node_id get_node_id() const
        { return nodeId_; }

    auto get_group() const -> const context&
        { return group_; }

    auto get_group() -> context&
        { return group_; }

protected:
    auto get_graph() const -> const react_graph&
        { return get_internals( group_ ).get_graph(); }

    auto get_graph() -> react_graph&
        { return get_internals( group_ ).get_graph(); }

    void attach_to_me(node_id otherNodeId)
        { get_graph().attach_node(nodeId_, otherNodeId); }

    void detach_from_me(node_id otherNodeId)
        { get_graph().detach_node(nodeId_, otherNodeId); }

private:
    context group_;
    node_id nodeId_;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_NODE_BASE_H_INCLUDED