
//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_GROUP_H_INCLUDED
#define REACT_GROUP_H_INCLUDED

#pragma once

#include "react/detail/defs.h"

#include <memory>
#include <utility>

#include "react/api.h"
#include "react/common/syncpoint.h"

#include "react/detail/graph_interface.h"
#include "react/detail/graph_impl.h"

/*****************************************/ REACT_BEGIN /*****************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Group
///////////////////////////////////////////////////////////////////////////////////////////////////
class group : protected REACT_IMPL::group_internals
{
public:
    group() = default;

    group(const group&) = default;
    group& operator=(const group&) = default;

    group(group&&) = default;
    group& operator=(group&&) = default;

    template <typename F>
    void do_transaction(F&& func)
        { get_graph_ptr()->do_transaction(std::forward<F>(func)); }

    template <typename F>
    void enqueue_transaction(F&& func, TransactionFlags flags = TransactionFlags::none )
        { get_graph_ptr()->enqueue_transaction(std::forward<F>(func), SyncPoint::Dependency{ }, flags); }

    template <typename F>
    void enqueue_transaction(F&& func, const SyncPoint& syncPoint, TransactionFlags flags = TransactionFlags::none )
        { get_graph_ptr()->enqueue_transaction(std::forward<F>(func), SyncPoint::Dependency{ syncPoint }, flags); }

    friend bool operator==(const group& a, const group& b)
        { return a.get_graph_ptr() == b.get_graph_ptr(); }

    friend bool operator!=(const group& a, const group& b)
        { return !(a == b); }

    friend auto get_internals( group& g) -> REACT_IMPL::group_internals&
        { return g; }

    friend auto get_internals(const group& g) -> const REACT_IMPL::group_internals&
        { return g; }
};

/******************************************/ REACT_END /******************************************/

#endif // REACT_GROUP_H_INCLUDED